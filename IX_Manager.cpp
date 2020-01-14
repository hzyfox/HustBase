#include "stdafx.h"
#include "IX_Manager.h"

RC OpenIndexScan(IX_IndexScan *indexScan,IX_IndexHandle *indexHandle,CompOp compOp,char *value){
	return SUCCESS;
}

RC IX_GetNextEntry(IX_IndexScan *indexScan,RID * rid){
	return SUCCESS;
}

RC CloseIndexScan(IX_IndexScan *indexScan){
		return SUCCESS;
}

RC GetIndexTree(char *fileName, Tree *index){
		return SUCCESS;
}


RC InsertEntry(IX_IndexHandle* indexHandle, void* pData, const RID* rid) {
	IX_PageHandle* ixPageHandle;
	if (indexHandle->ixPageHandle.pageHandle.pFrame->page.pageNum
		== indexHandle->ixPageHandle.fileHeader->rootPage) {
		FindLeafPage(indexHandle, &indexHandle->ixPageHandle, &ixPageHandle, pData, rid);
	}else {
		IX_PageHandle root;
		GetIndexPage(indexHandle, indexHandle->ixPageHandle.fileHeader->rootPage, &root);
		FindLeafPage(indexHandle, &root, &ixPageHandle, pData, rid);
		IXUnpinPage(&root);
	}

	


}

void IXUnpinPage(IX_PageHandle* pageHandle) {
	if (pageHandle->pageHandle.pFrame->page.pageNum != 1) {
		UnpinPage(&pageHandle->pageHandle);
	}
	return;
}

RC FindLeafPage(IX_IndexHandle * ixIndexHandle,IX_PageHandle* root,
	IX_PageHandle** res, void* pData, const RID* rid) {
	const IX_FileHeader* ixFileHeader = ixIndexHandle->ixPageHandle.fileHeader;
	char* keys = root->ixNode->keys;
	RID* rids = root->ixNode->rids;
	int keynum = root->ixNode->keynum;

	//这里可以用二分， 为了实现简单，先不用
	if (root->ixNode->is_leaf) {
		(*res) = root;
		return SUCCESS;
	}
	IX_PageHandle tmp_root;
	if (cmpAttr(ixFileHeader->attrType, ixFileHeader->attrLength, pData, rid, keys) < 0) {
		GetIndexPage(ixIndexHandle,rids->pageNum, &tmp_root);
		//dirty code, 为了保证ixPageHandle存留在缓存中，不被替换出去
	
		IXUnpinPage(root);
		
		return FindLeafPage(ixIndexHandle, &tmp_root, res, pData, rid);
	}
	if (cmpAttr(ixFileHeader->attrType, ixFileHeader->attrLength, pData, rid, keys +
		(keynum-1) * ixFileHeader->keyLength) >= 0) {
		GetIndexPage(ixIndexHandle, (rids+keynum)->pageNum, &tmp_root);
		//dirty code, 为了保证ixPageHandle存留在缓存中，不被替换出去
		IXUnpinPage(root);
		return FindLeafPage(ixIndexHandle, &tmp_root, res, pData, rid);
	}
	int prev = 0;
	for (int i = 0; i<keynum-1; ++i) {
		int first = cmpAttr(ixFileHeader->attrType, ixFileHeader->attrLength, pData,
			rid, keys + i * ixFileHeader->keyLength);
		int second = cmpAttr(ixFileHeader->attrType, ixFileHeader->attrLength, pData,
			rid, keys + (i + 1) * ixFileHeader->keyLength);
		if (first >= 0 && second < 0) {
			GetIndexPage(ixIndexHandle, (rids + i + 1)->pageNum, &tmp_root);
			//dirty code, 为了保证ixPageHandle存留在缓存中，不被替换出去
			IXUnpinPage(root);
			return FindLeafPage(ixIndexHandle, &tmp_root, res, pData, rid);
		}
	}
	return SUCCESS;
}
int cmpAttr(AttrType type, int attrLength, void* lpData, const RID* lrid, char* keys) {
	void* rpData = keys;
	const RID* rrid = (RID*)((char*)rpData + attrLength);
	int cmp;
	float cmp0;
	switch (type) {
	case chars:
		cmp = mystrcmp((char*)lpData, (char*)rpData, attrLength);
		break;
	case floats:
		cmp0 = *((float*)lpData) - *((float*)rpData);
		if (abs(cmp0) < FLT_EPSILON) {
			cmp = 0;
		}
		else {
			if (*((float*)lpData) > * ((float*)rpData))
				cmp = 1;
			else
				cmp = -1;
		}
		break;
	case ints:
		cmp = *((int*)lpData) - *((int*)rpData);
		break;
	default:
		perror("unknown attrType");
		return -2;
		break;
	}
	if (cmp == 0) {
		if (lrid->pageNum < rrid->pageNum) {
			return -1;
		}
		if (lrid->pageNum > rrid->pageNum) {
			return 1;
		}
		if (lrid->slotNum < rrid->slotNum) {
			return 1;
		}
		if (lrid->slotNum > rrid->slotNum) {
			return 1;
		}
		perror("索引已经存在");
		return -3;
	}
	else if (cmp > 0) {
		return 1;
	}
	else {
		return -1;
	}
}

int mystrcmp(char* lstr, char* rstr, int length) {
	for (int i = 0; i < length; ++i) {
		if (lstr[i] >rstr[i]) {
			return 1;
		}
		if (lstr[i] < rstr[i]) {
			return -1;
		}
	}
	return 0;
}

RC DeleteEntry(IX_IndexHandle* indexHandle, void* pData, const RID* rid) {
	return SUCCESS;
}

RC GetIndexPage(IX_IndexHandle* indexHandle, PageNum page_num, IX_PageHandle* ixPageHandle) {
	PF_PageHandle pfPageHandle;
	RC tmp;
	if ((tmp = GetThisPage(&(indexHandle->fileHandle), 
		indexHandle->ixPageHandle.fileHeader->rootPage, &pfPageHandle))!= SUCCESS) {
		return tmp;
	}
	ixPageHandle->pageHandle = pfPageHandle;
	ixPageHandle->fileHeader = (IX_FileHeader*)((char*)ixPageHandle->
		pageHandle.pFrame->page.pData);
	ixPageHandle->ixNode = (IX_Node*)((char*)ixPageHandle->
		pageHandle.pFrame->page.pData + sizeof(IX_FileHeader));
	return SUCCESS;
}


RC OpenIndex(const char* fileName, IX_IndexHandle* indexHandle) {
	RC tmp;
	if ((tmp = openFile((char*)fileName, &indexHandle->fileHandle)) != SUCCESS) {
		return tmp;
	}
	indexHandle->bOpen = true;
	PF_PageHandle  _pageHandle;
	PF_PageHandle* pageHandle = &_pageHandle;
	if ((tmp = GetThisPage(&(indexHandle->fileHandle),
		indexHandle->fileHandle.pHdrFrame->page.pageNum + 1, pageHandle)) != SUCCESS) {
		return tmp;
	}
	//IX_FileHeader* ixFileHeader = (IX_FileHeader*)pageHandle->pFrame->page.pData;
	//indexHandle->frame = pageHandle->pFrame;
	//indexHandle->ixNode = (IX_Node*)((char*)pageHandle->pFrame->page.pData + sizeof(IX_FileHeader));
	/*indexHandle->ixPageHandle.fileHeader = (IX_FileHeader*)(pageHandle->pFrame->page.pData);
	indexHandle->ixPageHandle.ixNode = (IX_Node*)((char*)pageHandle->pFrame->page.pData + sizeof(IX_FileHeader));
	indexHandle->ixPageHandle.pageHandle = *pageHandle;*/
	ConfigIndexPageHandle(*pageHandle, indexHandle, &indexHandle->ixPageHandle);
	return SUCCESS;
}
//由于指导书中B+ 数 节点个数和指针个数相等的形式，在根节点扩容的时候需要产生2个key，
//且插入最小值得时候，需要更新索引节点的最小值，所以我们这里不采用指导书中的定义
//采用wiki上的定义 http://www.cburch.com/cs/340/reading/btree/index.html
//Every node has one more references than it has keys.
//All leaves are at the same distance from the root.
//For every non - leaf node N with k being the number of keys in N : all keys in the first child's subtree are less than N's first key;and all keys in the ith child's subtree (2 ≤ i ≤ k) are between the (i − 1)th key of n and the ith key of n.
//The root has at least two children.
//Every non - leaf, non - root node has at least floor(d / 2) children.
//Each leaf contains at least floor(d / 2) keys.
//Every key from the table appears in a leaf, in left - to - right sorted order.
RC CreateIndex(const char* fileName, AttrType attrType, int attrLength) {
	int fd;
	PF_FileSubHeader* fileSubHeader;
	fd = _open(fileName, _O_RDWR | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
	if (fd < 0)
		return IF_EXIST;
	_close(fd);
	char* bitmap;
	fd = open(fileName, _O_RDWR);
	Page page0;
	memset(&page0, 0, PF_PAGESIZE);
	page0.pageNum = 0;
	bitmap = page0.pData + (int)PF_FILESUBHDR_SIZE;
	fileSubHeader = (PF_FileSubHeader*)page0.pData;
	fileSubHeader->nAllocatedPages = 1;
	fileSubHeader->pageCount = 0;
	bitmap[0] |= 0x01;

	Page page1;
	memset(&page1, 0, PF_PAGESIZE);
	fileSubHeader->nAllocatedPages = 2;
	fileSubHeader->pageCount = 1;
	page1.pageNum = 1;
	bitmap[0] |= 0x2;
	
	IX_FileHeader* ixFileHeader = (IX_FileHeader*)page1.pData;
	IX_Node* ixNode = (IX_Node*)((char*)page1.pData + sizeof(IX_FileHeader));
	ixFileHeader->attrLength = attrLength;
	ixFileHeader->attrType = attrType;
	ixFileHeader->keyLength = attrLength + sizeof(RID);//一个key的长度和一个RID结合起来 区别重复的Key
	ixFileHeader->first_leaf = 1;
	ixFileHeader->rootPage = 1;
	ixFileHeader->order = ((PF_PAGE_SIZE - sizeof(IX_FileHeader)
		- sizeof(IX_Node)-sizeof(RID)) / (ixFileHeader->keyLength + sizeof(RID))) + 1;
	
	ixNode->brother = NO_BROTHER; //暂定-1代表没有右兄弟
	ixNode->is_leaf = 1;
	ixNode->keynum = 0;
	ixNode->keys = (char*)page1.pData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	
		
	ixNode->rids = (RID*)(ixNode->keys + (ixFileHeader->order-1) * ixFileHeader->keyLength);
	ixNode->parent = NO_PARENT;

	if (_lseek(fd, 0, SEEK_SET) == -1)
		return IF_FILEERR;
	if (_write(fd, (char*)& page0, sizeof(Page)) != sizeof(Page)) {
		_close(fd);
		return IF_FILEERR;
	}
	if (_write(fd, (char*)& page1, sizeof(Page)) != sizeof(Page)) {
		_close(fd);
		return IF_FILEERR;
	}
	return SUCCESS;
}

void ConfigIndexPageHandle(PF_PageHandle pageHandle, IX_IndexHandle* indexHandle, IX_PageHandle * ixPageHandle) {
	ixPageHandle->pageHandle = pageHandle;
	ixPageHandle->fileHeader = (IX_FileHeader*)(ixPageHandle->pageHandle.pFrame->page.pData);
	ixPageHandle->ixNode = (IX_Node*)((char*)ixPageHandle->pageHandle.pFrame
		->page.pData + sizeof(IX_FileHeader));
}

RC CloseIndex(IX_IndexHandle* indexHandle) {
	RC tmp;
	indexHandle->bOpen = false;
	_ASSERT_EXPR(indexHandle->fileHandle.pHdrFrame->pinCount == 1, "pinCount should be 1");
	indexHandle->fileHandle.pHdrFrame->pinCount--;
	if ((tmp = CloseFile(&indexHandle->fileHandle)) != SUCCESS) {
		return tmp;
	}
	return SUCCESS;

}


