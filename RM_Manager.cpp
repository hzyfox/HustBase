#include "stdafx.h"
#include "RM_Manager.h"
#include "str.h"
#include "PF_Manager.h"

//初始化人rmFileScan， 准备扫描每一个插槽
RC OpenScan(RM_FileScan *rmFileScan,RM_FileHandle *fileHandle,int conNum,Con *conditions)
{
	rmFileScan->bOpen = true;
	rmFileScan->conNum = conNum;
	rmFileScan->conditions = conditions;
	rmFileScan->pRMFileHandle = fileHandle;
	unsigned int i = 0;
	//我们这里先不get pagehandle, 到GetNextRec的时候再get。否则 pincount 会被增加两次。
	for (i = 2; i <= fileHandle->pFileHandler->pFileSubHeader->pageCount; ++i) {
		if (fileHandle->pFileHandler->pBitmap[i / 8] & (1 << (i % 8))) {
			rmFileScan->pn = i;
			rmFileScan->sn = 0;
			break;
		}
	}
	if (i > fileHandle->pFileHandler->pFileSubHeader->pageCount) {
		perror("init rmFileScan failed");
		return RM_EOF;
	}
	
	return SUCCESS;
}

RC GetNextRec(RM_FileScan *rmFileScan,RM_Record *rec)
{
	RM_FileHandle* rmFileHandle = rmFileScan->pRMFileHandle;
	int pageCount = rmFileHandle->pFileHandler->pFileSubHeader->pageCount;
	int i = 0;
	for (i = rmFileScan->pn; i <= pageCount; ++i) {
		rmFileScan->pn = i;
		//如果没有allocted 跳过
		if (!(rmFileHandle->pFileHandler->pBitmap[i / 8] & (1 << (i % 8)))) {
			continue;
		}
		if (GetThisPage(rmFileScan->pRMFileHandle->pFileHandler, i, &(rmFileScan->PageHandle)) != SUCCESS) {
			continue;
		}
		//PF_PageHandle pageHandle = rmFileScan->PageHandle; 不能这么传递，会变成值传递
		RM_PageHandle rmPageHandle;
		ConfigRMPageHandle(&rmPageHandle, rmFileHandle, &rmFileScan->PageHandle);
		int recordPerPage = rmFileHandle->pRecordFileSubHeader->recordsPerPage;
		char* pageBitmap = rmPageHandle.bitmap;
		int recordSize = rmFileHandle->pRecordFileSubHeader->recordSize;
		for (int j = rmFileScan->sn; j < recordPerPage; ++j) {
			rmFileScan->sn = j;
			if (!(pageBitmap[j / 8] & (1 << (j % 8)))) {
				continue;
			}
			Con* con = rmFileScan->conditions;
			bool res = true;
			rec->pData = rmPageHandle.data + j * recordSize;//这里先浅拷贝，进行比较
			
			for (int k = 0; k < rmFileScan->conNum; ++k) {
				res &= compSingleCondition(con + k, rec);
				if (!res) {
					break;
				}
			}
			if (res) {
				rec->rid.bValid = true;
				rec->rid.pageNum = i;
				rec->rid.slotNum = j;
				rec->bValid = true;
				if (j + 1 == recordPerPage) {
					rmFileScan->pn = i+1;
					rmFileScan->sn = 0;
				}else {
					rmFileScan->pn = i;
					rmFileScan->sn = j + 1;
				}
				//符合条件时，再进行深拷贝
				rec->pData = (char*)(malloc(recordSize));
				memcpy(rec->pData, rmPageHandle.data + j * recordSize, recordSize);
				UnpinPage(&rmFileScan->PageHandle);
				return SUCCESS;
			}
		}
		UnpinPage(&rmFileScan->PageHandle);
	}

	return RM_EOF;
}

RC GetRec (RM_FileHandle *fileHandle,RID *rid, RM_Record *rec) 
{
	if (!rid->bValid) {
		return RM_INVALIDRID;
	}
	RC tmp;
	//int byte, bit;
	//fileHandle->pRecFrame->bDirty = true;
	
	RM_PageHandle* rmPageHandle = getRM_PageHanle();
	if ((tmp = RM_GetThisPage(fileHandle, rid->pageNum, rmPageHandle)) != SUCCESS) {
		return tmp;
	}
	rec->bValid = true;
	rec->rid = *rid;
	int recordSize = fileHandle->pRecordFileSubHeader->recordSize;
	rec->pData = (char*)malloc(recordSize);
	if (!(rmPageHandle->bitmap[rid->slotNum / 8] & (1 << (rid->slotNum % 8)))) {
		return RM_INVALIDRID;
	}
	memcpy(rec->pData, 
		rmPageHandle->data + rid->slotNum * recordSize, recordSize);
	UnpinPage(rmPageHandle->pageHandle);
	free(rmPageHandle->pageHandle);
	free(rmPageHandle);
	return SUCCESS;
}

RC InsertRec (RM_FileHandle *fileHandle,char *pData, RID *rid)
{
	RM_PageHandle  _rmPageHandle;
	RM_PageHandle* rmPageHandle = &_rmPageHandle;
	RC tmp;
	if ((tmp = RM_AllocatePage(fileHandle, rmPageHandle)) != SUCCESS) {
		return tmp;
	}

	int i, byte, bit;
	fileHandle->pRecFrame->bDirty = true;
	for (i = 0; i < fileHandle->pRecordFileSubHeader->recordsPerPage; ++i) {
		byte = i / 8;
		bit = i % 8;
		if ((rmPageHandle->bitmap[byte] & (1 << bit)) == 0) {
			/*rmPageHandle->pageHandle->pFrame->bDirty = true;*/
			MarkDirty(rmPageHandle->pageHandle);
			memcpy(rmPageHandle->data + (i * rmPageHandle->recordSize), pData, rmPageHandle->recordSize);
			_ASSERT_EXPR(equalStr(rmPageHandle->data + (i * rmPageHandle->recordSize), pData,
				rmPageHandle->recordSize), "insert incorrect position");
			rmPageHandle->bitmap[byte] |= (1 << bit);
			_ASSERT_EXPR(rmPageHandle->bitmap[byte] & (1 << bit), "page bitmap set failed");
			fileHandle->pRecordFileSubHeader->nRecords++;
			rid->bValid = true;
			rid->pageNum = rmPageHandle->pageHandle->pFrame->page.pageNum;
			int count = RM_BitCount(rmPageHandle->bitmap, rmPageHandle->firstRecordOffset);
			if (count >= rmPageHandle->recordPerPage) {
				fileHandle->pRecordBitmap[(rid->pageNum)/8] |= (1 << ((rid->pageNum) % 8));
				_ASSERT_EXPR(fileHandle->pRecordBitmap[(rid->pageNum)/8] & (1 << ((rid->pageNum)%8)), "record bitmap set failed");
			}
			rid->slotNum = i;
			_ASSERT_EXPR(fileHandle->pRecFrame->bDirty, "fileHandle->pRecFrame->bDirty is false");
			UnpinPage(rmPageHandle->pageHandle);
			_ASSERT_EXPR(rmPageHandle->pageHandle->pFrame->pinCount == 0, "pincount !=0");
			free(rmPageHandle->pageHandle);
			return SUCCESS;
		}
	}
	if (i == fileHandle->pRecordFileSubHeader->recordsPerPage) {
		perror("insert Rec failed");
		return FAIL;
	}
	UnpinPage(rmPageHandle->pageHandle);
	_ASSERT_EXPR(rmPageHandle->pageHandle->pFrame->pinCount == 0, "pincount !=0");
	free(rmPageHandle->pageHandle);
	return SUCCESS;
}

RC DeleteRec (RM_FileHandle *fileHandle,const RID *rid)
{
	if (!rid->bValid) {
		return RM_INVALIDRID;
	}
	RC tmp;
	int byte, bit;
	fileHandle->pRecFrame->bDirty = true;
	byte = rid->slotNum / 8;
	bit = rid->slotNum % 8;
	RM_PageHandle* rmPageHandle = getRM_PageHanle();
	if ((tmp = RM_GetThisPage(fileHandle, rid->pageNum, rmPageHandle)) != SUCCESS) {
		return tmp;
	}
	//rmPageHandle->pageHandle->pFrame->bDirty = true;

	MarkDirty(rmPageHandle->pageHandle);
	rmPageHandle->bitmap[byte] &= ~(1 << bit);

	byte = rid->pageNum / 8;
	bit = rid->pageNum % 8;
	fileHandle->pRecordBitmap[byte] &= ~(1 << bit);
	_ASSERT_EXPR((fileHandle->pRecordBitmap[byte] & (1 << bit)) == 0, "delete, but bitmap not change");
	fileHandle->pRecordFileSubHeader->nRecords--;
	UnpinPage(rmPageHandle->pageHandle);
	free(rmPageHandle->pageHandle);
	free(rmPageHandle);
	
	return SUCCESS;
}

RC UpdateRec (RM_FileHandle *fileHandle,const RM_Record *rec)
{
	if (!rec->bValid) {
		return RM_INVALIDRID;
	}
	RC tmp;
	const RID* rid = &(rec->rid);
	int byte, bit;
	byte = rid->slotNum / 8;
	bit = rid->slotNum % 8;
	RM_PageHandle* rmPageHandle = getRM_PageHanle();
	if ((tmp = RM_GetThisPage(fileHandle, rid->pageNum, rmPageHandle)) != SUCCESS) {
		return tmp;
	}
	MarkDirty(rmPageHandle->pageHandle);
	memcpy(rmPageHandle->data+rec->rid.slotNum * rmPageHandle->recordSize, rec->pData, fileHandle->pRecordFileSubHeader->recordSize);
	UnpinPage(rmPageHandle->pageHandle);
	free(rmPageHandle->pageHandle);
	free(rmPageHandle);
	return SUCCESS;
}

//创建File，并且写入页面管理的元数据信息，和记录管理的元数据信息。
// 需要改变的元信息： PF_FileSubHeader 中的 pageCount ++ nAllocatedPage++;
//				     RM_FileSubHeader 中的信息
//					 记录位图，代表哪些是满页，哪些是非满页
					 
RC RM_CreateFile (char *fileName, int recordSize)
{
	RC tmp;
	char* recBitmap;
	RM_FileSubHeader* rmFileSubHeader;

	if ((tmp = CreateFile(fileName)) != SUCCESS) {
		return tmp;
	}

	PF_FileHandle* pfFileHeader = getPF_FileHandle();

	//因为需要修改allocatedPage
	if ((tmp = openFile(fileName, pfFileHeader)) != SUCCESS) {
		return tmp;
	}
	
	Frame* headFrame = pfFileHeader->pHdrFrame;
	headFrame->bDirty = true;

	PF_PageHandle* pageHandle = getPF_PageHandle();

	if (pageHandle == NULL) {
		perror("RM_CreateFile malloc pageHandle failed");
		return tmp;
	}

	if ((tmp = AllocatePage(pfFileHeader, pageHandle)) != SUCCESS) {
		return tmp;
	}
	
	recBitmap =(char*) pageHandle->pFrame->page.pData + RM_FILESUBHDR_SIZE;
	recBitmap[0] |= 1 << 0; //第0页是页面管理器元信息
	recBitmap[0] |= 1 << 1; //第1页是记录管理器元信息

	rmFileSubHeader = (RM_FileSubHeader*)pageHandle->pFrame->page.pData;
	rmFileSubHeader->recordSize = recordSize;
	rmFileSubHeader->nRecords = 0;
	//假设位图占x字节， 则 x + 8x*recordSize = PF_PAGE_SIZE; 位图需要向上取整，才能保证能记录所有record
	
	rmFileSubHeader->firstRecordOffset = (int)ceil((float)PF_PAGE_SIZE
		/ (float)(8 * recordSize + 1));
	// 算出位图的Offset后， 剩下的空间就是能够存放的记录数， 向下取整，因为记录不能半途而废
	rmFileSubHeader->recordsPerPage = (PF_PAGE_SIZE - rmFileSubHeader->firstRecordOffset)
		/ recordSize;

	MarkDirty(pageHandle);
	UnpinPage(pageHandle);
	if (CloseFile(pfFileHeader) != SUCCESS) {
		return PF_FILEERR;
	};
	free(pfFileHeader);
	free(pageHandle);

	return SUCCESS;
}


//根据文件名打开指定的记录文件，返回其文件句柄指针
//需要改变的元数据信息： 
RC RM_OpenFile(char *fileName, RM_FileHandle *fileHandle)
{
	PF_FileHandle* pfFileHeader = getPF_FileHandle();
	RC tmp;
	if ((tmp = openFile(fileName, pfFileHeader)) != SUCCESS) {
		return tmp;
	}
	fileHandle->bOpen = true;
	fileHandle->pFileHandler = pfFileHeader;
	PF_PageHandle* pageHandle = getPF_PageHandle();
	RM_PageHandle* rmPageHandle = getRM_PageHanle();
	if ((tmp = GetThisPage(pfFileHeader, pfFileHeader->pHdrFrame->page.pageNum + 1, pageHandle))!=SUCCESS) {
		return tmp;
	}
	fileHandle->pRecFrame = pageHandle->pFrame;


	fileHandle->pRecordFileSubHeader = (RM_FileSubHeader*)pageHandle->pFrame->page.pData;
	fileHandle->pRecPage = &(pageHandle->pFrame->page);
	fileHandle->pRecordBitmap = fileHandle->pRecFrame->page.pData + RM_FILESUBHDR_SIZE;
	
	ConfigRMFilePageHandle(rmPageHandle, fileHandle, pageHandle);
	fileHandle->rmPageHandle = rmPageHandle;

	return SUCCESS;
}
//这里保留 fileHandle 不free 因为测试程序需要测试关闭了文件之后 功能是否正常
RC RM_CloseFile(RM_FileHandle *fileHandle)
{
	RC tmp;
	fileHandle->pRecFrame->pinCount--;
	if ((tmp = CloseFile(fileHandle->pFileHandler)) != SUCCESS) {
		return tmp;
	}
	
	


	return SUCCESS;
}

//寻找非满页, 寻找已经allocated的但是未满的。如果不存在没有allocated但是未满的则扩充文件新建一个页面

RC RM_AllocatePage(RM_FileHandle* fileHandle, RM_PageHandle* rmPageHandle) {
	if (!fileHandle->bOpen) {
		return RM_FHCLOSED;
	}
	//TODO: 这里如何避免重复申请PageHandle
	PF_PageHandle* pfPageHandle = getPF_PageHandle();
	PF_FileHandle* pFileHandler = fileHandle->pFileHandler;
	char* PF_bitmap = pFileHandler->pBitmap;
	char* rec_bitmap = fileHandle->pRecordBitmap;
	int byte, bit;
	unsigned int i;
	RC tmp;
	for (i = 0; i <= pFileHandler->pFileSubHeader->pageCount; i++) {
		byte = i / 8;
		bit = i % 8;
		//注意这里不能判断==1,应该判断==(1<<bit)
		bool allocated = ((PF_bitmap[byte] & (1 << bit)) != 0);
		bool full = ((rec_bitmap[byte] & (1 << bit)) !=0);
		
		if (full) {
			continue;
		}
		if (allocated) {
			if ((tmp = GetThisPage(pFileHandler, i, pfPageHandle)) != SUCCESS) {
				return tmp;
			}
			ConfigRMPageHandle(rmPageHandle, fileHandle, pfPageHandle);
			return SUCCESS;
		}else {
			pFileHandler->pHdrFrame->bDirty=true;
			(pFileHandler->pFileSubHeader->nAllocatedPages)++;
			pFileHandler->pBitmap[byte] |= (1 << bit);
			if ((tmp = GetThisPage(pFileHandler, i, pfPageHandle)) != SUCCESS) {
				return tmp;
			}
			ConfigRMPageHandle(rmPageHandle, fileHandle, pfPageHandle);
			return SUCCESS;
		}
	}

	if ((tmp = AllocateNewPage(pFileHandler, pfPageHandle)) != SUCCESS) {
		return tmp;
	}
	ConfigRMPageHandle(rmPageHandle, fileHandle, pfPageHandle);
	return SUCCESS;
}


RC RM_GetThisPage(RM_FileHandle* rmFileHandle, PageNum pageNum, RM_PageHandle* rmPageHandle) {
	PF_FileHandle* pfFileHandle = rmFileHandle->pFileHandler;
	PF_PageHandle* pfPageHandle = getPF_PageHandle();
	RC tmp;
	if ((tmp = GetThisPage(pfFileHandle, pageNum, pfPageHandle)) != SUCCESS) {
		return tmp;
	}
	ConfigRMPageHandle(rmPageHandle, rmFileHandle, pfPageHandle);
	return SUCCESS;
}

RM_PageHandle* getRM_PageHanle() {
	RM_PageHandle* p = (RM_PageHandle*)malloc(sizeof(RM_PageHandle));
	return p;
}

int RM_BitCount(char* bitmap, int size) {
	int count = 0;
	for (int i = 0; i < size; i++) {
		int c = 0;
		char tmp = *(bitmap + i);
		for (c = 0; tmp; ++c) {
			tmp &= (tmp - 1);
		}
		count += c;
	}
	return count;
}

void ConfigRMPageHandle(RM_PageHandle* rmPageHandle, RM_FileHandle* rmFileHandle, PF_PageHandle* pfPageHandle) {
	rmPageHandle->pageHandle = pfPageHandle;
	rmPageHandle->data = rmPageHandle->pageHandle->pFrame->page.pData + rmFileHandle->pRecordFileSubHeader->firstRecordOffset;
	rmPageHandle->bitmap = rmPageHandle->pageHandle->pFrame->page.pData;
	rmPageHandle->firstRecordOffset = rmFileHandle->pRecordFileSubHeader->firstRecordOffset;
	rmPageHandle->recordPerPage = rmFileHandle->pRecordFileSubHeader->recordsPerPage;
	rmPageHandle->recordSize = rmFileHandle->pRecordFileSubHeader->recordSize;
}

void ConfigRMFilePageHandle(RM_PageHandle* rmPageHandle, RM_FileHandle* rmFileHandle, PF_PageHandle* pfPageHandle) {
	rmPageHandle->pageHandle = pfPageHandle;
	rmPageHandle->data = NULL; //控制页没有data 
	rmPageHandle->bitmap = rmPageHandle->pageHandle->pFrame->page.pData + RM_FILESUBHDR_SIZE;
	rmPageHandle->firstRecordOffset = rmFileHandle->pRecordFileSubHeader->firstRecordOffset;
	rmPageHandle->recordPerPage = rmFileHandle->pRecordFileSubHeader->recordsPerPage;
	rmPageHandle->recordSize = rmFileHandle->pRecordFileSubHeader->recordSize;
}



bool compSingleCondition(const Con* con, const RM_Record* record) {
	void* lVal,  *rVal;
	if (con->bLhsIsAttr) {
		lVal = record->pData + con->LattrOffset;
	}else {
		lVal = con->Lvalue;
	}

	if (con->bRhsIsAttr) {
		rVal = record->pData + con->RattrOffset;
	}
	else {
		rVal = con->Rvalue;
	}
	int cmp;
	float cmp0;
	switch (con->attrType) {
	case chars:
		cmp = strcmp((char*)lVal, (char*)rVal);
		break;
	case floats:
		cmp0 = *((float*)lVal) - *((float*)rVal);
		if (abs(cmp0) < FLT_EPSILON) {
			cmp = 0;
		}
		else {
			if (*((float*)lVal) > * ((float*)rVal))
				cmp = 1;
			else
				cmp = -1;
		}
		break;
	case ints:
		cmp = *((int*)lVal) - *((int*)rVal);
		break;
	default:
		perror("unknown attrType");
		return false;
		break;
	}
	switch (con->compOp)
	{
	case EQual:
		return cmp == 0;
	case LessT:
		return cmp < 0;
	case GreatT:
		return cmp > 0;
	case LEqual:
		return cmp <= 0;
	case GEqual:
		return cmp >= 0;
	case NEqual:
		return cmp != 0;
	case NO_OP:
		return true;
	default:
		return false;
	}
}

bool equalStr(char* str0, char* str1, int len) {
	
	for (int i = 0; i < len; ++i) {
		if (str1[i] != str0[i]) {
			return false;
		}
	}
	return true;
}





