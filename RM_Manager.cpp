#include "stdafx.h"
#include "RM_Manager.h"
#include "str.h"
#include "PF_Manager.h"

RC OpenScan(RM_FileScan *rmFileScan,RM_FileHandle *fileHandle,int conNum,Con *conditions)//初始化扫描
{
	return SUCCESS;
}

RC GetNextRec(RM_FileScan *rmFileScan,RM_Record *rec)
{
	return SUCCESS;
}

RC GetRec (RM_FileHandle *fileHandle,RID *rid, RM_Record *rec) 
{
	return SUCCESS;
}

RC InsertRec (RM_FileHandle *fileHandle,char *pData, RID *rid)
{
	return SUCCESS;
}

RC DeleteRec (RM_FileHandle *fileHandle,const RID *rid)
{
	return SUCCESS;
}

RC UpdateRec (RM_FileHandle *fileHandle,const RM_Record *rec)
{
	return SUCCESS;
}

//创建File，并且写入页面管理的元数据信息，和记录管理的元数据信息。
// 需要改变的元信息： PF_FileSubHeader 中的 pageCount ++ nAllocatedPage++;
//				     RM_FileSubHeader 中的信息
					 
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
	

	PF_PageHandle* pageHandle = (PF_PageHandle*)(malloc(sizeof(PF_PageHandle)));

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
	rmFileSubHeader->firstRecordOffset = (int)ceilf((float)PF_PAGE_SIZE
		/ (float)(sizeof(char) * recordSize + 1));
	// 算出位图的Offset后， 剩下的空间就是能够存放的记录数， 向下取整，因为记录不能半途而废
	rmFileSubHeader->recordsPerPage = (PF_PAGE_SIZE - rmFileSubHeader->firstRecordOffset)
		/ sizeof(char);

	MarkDirty(pageHandle);

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

	if ((tmp = GetThisPage(pfFileHeader, pfFileHeader->pHdrFrame->page.pageNum + 1, pageHandle))!=SUCCESS) {
		return tmp;
	}
	fileHandle->pRecFrame = pageHandle->pFrame;
	fileHandle->pRecordFileSubHeader = (RM_FileSubHeader*)pageHandle->pFrame->page.pData;
	fileHandle->pRecPage = &(pageHandle->pFrame->page);
	fileHandle->pRecordBitmap = fileHandle->pRecFrame->page.pData + RM_FILESUBHDR_SIZE;
	free(pageHandle);
	return SUCCESS;
}

RC RM_CloseFile(RM_FileHandle *fileHandle)
{
	RC tmp;
	if ((tmp = CloseFile(fileHandle->pFileHandler)) != SUCCESS) {
		return tmp;
	}
	fileHandle->pRecFrame->pinCount--;
	return SUCCESS;
}


