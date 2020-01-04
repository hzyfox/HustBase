#include "stdafx.h"
#include "RM_Manager.h"
#include "str.h"
#include "PF_Manager.h"

RC OpenScan(RM_FileScan *rmFileScan,RM_FileHandle *fileHandle,int conNum,Con *conditions)//��ʼ��ɨ��
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

//����File������д��ҳ������Ԫ������Ϣ���ͼ�¼�����Ԫ������Ϣ��
// ��Ҫ�ı��Ԫ��Ϣ�� PF_FileSubHeader �е� pageCount ++ nAllocatedPage++;
//				     RM_FileSubHeader �е���Ϣ
					 
RC RM_CreateFile (char *fileName, int recordSize)
{
	RC tmp;
	char* recBitmap;
	RM_FileSubHeader* rmFileSubHeader;

	if ((tmp = CreateFile(fileName)) != SUCCESS) {
		return tmp;
	}

	PF_FileHandle* pfFileHeader = getPF_FileHandle();

	//��Ϊ��Ҫ�޸�allocatedPage
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
	recBitmap[0] |= 1 << 0; //��0ҳ��ҳ�������Ԫ��Ϣ
	recBitmap[0] |= 1 << 1; //��1ҳ�Ǽ�¼������Ԫ��Ϣ

	rmFileSubHeader = (RM_FileSubHeader*)pageHandle->pFrame->page.pData;
	rmFileSubHeader->recordSize = recordSize;
	rmFileSubHeader->nRecords = 0;
	//����λͼռx�ֽڣ� �� x + 8x*recordSize = PF_PAGE_SIZE; λͼ��Ҫ����ȡ�������ܱ�֤�ܼ�¼����record
	rmFileSubHeader->firstRecordOffset = (int)ceilf((float)PF_PAGE_SIZE
		/ (float)(sizeof(char) * recordSize + 1));
	// ���λͼ��Offset�� ʣ�µĿռ�����ܹ���ŵļ�¼���� ����ȡ������Ϊ��¼���ܰ�;����
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


//�����ļ�����ָ���ļ�¼�ļ����������ļ����ָ��
//��Ҫ�ı��Ԫ������Ϣ�� 
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


