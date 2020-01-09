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

	return SUCCESS;

}
RC DeleteEntry(IX_IndexHandle* indexHandle, void* pData, const RID* rid) {
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
	IX_FileHeader* ixFileHeader = (IX_FileHeader*)pageHandle->pFrame->page.pData;
	indexHandle->frame = pageHandle->pFrame;
	indexHandle->ixNode = (IX_Node*)((char*)pageHandle->pFrame->page.pData + sizeof(IX_FileHeader));
	indexHandle->fileHeader = *ixFileHeader;
	return SUCCESS;
}

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
	ixFileHeader->order = (PF_PAGE_SIZE - sizeof(IX_FileHeader)
		- sizeof(IX_Node)) / (ixFileHeader->keyLength + sizeof(RID));
	
	ixNode->brother = NO_BROTHER; //暂定-1代表没有右兄弟
	ixNode->is_leaf = 1;
	ixNode->keynum = 0;
	ixNode->keys = (char*)page1.pData + sizeof(IX_FileHeader) + sizeof(IX_Node);
	int maxIndexNum = (PF_PAGE_SIZE - (ixNode->keys - page1.pData)) / (ixFileHeader->keyLength + sizeof(RID));
		
	ixNode->rids = (RID*)(ixNode->keys + maxIndexNum * ixFileHeader->keyLength);
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


