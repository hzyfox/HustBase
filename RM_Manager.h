#ifndef RM_MANAGER_H_H
#define RM_MANAGER_H_H

#include "PF_Manager.h"
#include "str.h"
#include<iostream>
using namespace std;
#define RM_FILESUBHDR_SIZE (sizeof(RM_FileSubHeader))
typedef int SlotNum;

//#ifdef _DEBUG
//#   define _ASSERT(condition, message) \
//    do { \
//        if (! (condition)) { \
//            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
//                      << " line " << __LINE__ << ": " << message << std::endl; \
//            std::terminate(); \
//        } \
//    } while (false)
//#else
//#   define ASSERT(condition, message) do { } while (false)
//#endif
typedef struct {
	int nRecords; //��ǰ�ļ��а����ļ�¼��
	int recordSize; //ÿ����¼�Ĵ�С
	int recordsPerPage; //ÿ��ҳ�����װ�ڵļ�¼����
	int firstRecordOffset; //ÿҳ��һ����¼���������Ŀ�ʼλ�ã��������ȥ��ÿҳ��λͼ֮���λ��
}RM_FileSubHeader;

typedef struct {
	PF_PageHandle* pageHandle;
	char* data;
	char* bitmap;
	int firstRecordOffset;
	int recordPerPage;
	int recordSize;
}RM_PageHandle;


typedef struct {//�ļ����
	bool bOpen;//����Ƿ�򿪣��Ƿ����ڱ�ʹ�ã�
	PF_FileHandle* pFileHandler; //�ļ����
	RM_FileSubHeader* pRecordFileSubHeader;//meta��Ϣ�ṹָ��
	char* pRecordBitmap; //��¼λͼָ��,��¼��ҳ������ҳ
	Frame* pRecFrame; //ָ���¼Ԫ��Ϣ֡
	Page* pRecPage; //ָ���¼Ԫ��Ϣҳ
	//��Ҫ�Զ������ڲ��ṹ
}RM_FileHandle;


typedef struct {	
	PageNum pageNum;	//��¼����ҳ��ҳ��
	SlotNum slotNum;		//��¼�Ĳ�ۺ�
	bool bValid; 			//true��ʾΪһ����Ч��¼�ı�ʶ��
}RID;

typedef struct{
	bool bValid;		 // False��ʾ��δ�������¼
	RID  rid; 		 // ��¼�ı�ʶ�� 
	char *pData; 		 //��¼���洢������ 
}RM_Record;


typedef struct
{
	int bLhsIsAttr,bRhsIsAttr;//���������ԣ�1������ֵ��0��
	AttrType attrType;
	int LattrLength,RattrLength;
	int LattrOffset,RattrOffset;
	CompOp compOp;
	void *Lvalue,*Rvalue;
}Con;



typedef struct{
	bool  bOpen;		//ɨ���Ƿ�� 
	RM_FileHandle  *pRMFileHandle;		//ɨ��ļ�¼�ļ����
	int  conNum;		//ɨ���漰���������� 
	Con  *conditions;	//ɨ���漰����������ָ��
    PF_PageHandle  PageHandle; //�����е�ҳ����
	PageNum  pn; 	//ɨ�輴�������ҳ���
	SlotNum  sn;		//ɨ�輴������Ĳ�ۺ�
}RM_FileScan;



RC GetNextRec(RM_FileScan *rmFileScan,RM_Record *rec);

RC OpenScan(RM_FileScan *rmFileScan,RM_FileHandle *fileHandle,int conNum,Con *conditions);

RC CloseScan(RM_FileScan *rmFileScan);

RC UpdateRec (RM_FileHandle *fileHandle,const RM_Record *rec);

RC DeleteRec (RM_FileHandle *fileHandle,const RID *rid);

RC InsertRec (RM_FileHandle *fileHandle, char *pData, RID *rid); 

RC GetRec (RM_FileHandle *fileHandle, RID *rid, RM_Record *rec); 

RC RM_CloseFile (RM_FileHandle *fileHandle);

RC RM_OpenFile (char *fileName, RM_FileHandle *fileHandle);

RC RM_CreateFile (char *fileName, int recordSize);

RM_PageHandle* getRM_PageHanle();
bool compSingleCondition(const Con* con, const RM_Record* record);
void ConfigRMPageHandle(RM_PageHandle* rmPageHandle, RM_FileHandle* rmFileHandle, PF_PageHandle* pfPageHandle);
int RM_BitCount(char* bitmap, int size);
RC RM_GetThisPage(RM_FileHandle* rmFileHandle, PageNum pageNum, RM_PageHandle* rmPageHandle);
RC RM_AllocatePage(RM_FileHandle* fileHandle, RM_PageHandle* rmPageHandle);

#endif