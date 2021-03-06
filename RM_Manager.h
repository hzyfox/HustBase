#ifndef RM_MANAGER_H_H
#define RM_MANAGER_H_H

#include "PF_Manager.h"
#include "str.h"
#include<iostream>
#include<array>
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
	int nRecords; //当前文件中包含的记录数
	int recordSize; //每个记录的大小
	int recordsPerPage; //每个页面可以装在的记录数量
	int firstRecordOffset; //每页第一个记录在数据区的开始位置，我理解是去除每页的位图之后的位置
}RM_FileSubHeader;

typedef struct {
	PF_PageHandle* pageHandle;
	char* data;
	char* bitmap;
	int firstRecordOffset;
	int recordPerPage;
	int recordSize;
}RM_PageHandle;


typedef struct {//文件句柄
	bool bOpen;//句柄是否打开（是否正在被使用）
	PF_FileHandle* pFileHandler; //文件句柄
	RM_FileSubHeader* pRecordFileSubHeader;//meta信息结构指针
	char* pRecordBitmap; //记录位图指针,记录满页，非满页
	Frame* pRecFrame; //指向记录元信息帧
	Page* pRecPage; //指向记录元信息页
	RM_PageHandle* rmPageHandle;
	//需要自定义其内部结构
}RM_FileHandle;


typedef struct {	
	PageNum pageNum;	//记录所在页的页号
	SlotNum slotNum;		//记录的插槽号
	bool bValid; 			//true表示为一个有效记录的标识符
}RID;

typedef struct{
	bool bValid;		 // False表示还未被读入记录
	RID  rid; 		 // 记录的标识符 
	char *pData; 		 //记录所存储的数据 
}RM_Record;


typedef struct
{
	int bLhsIsAttr,bRhsIsAttr;//左、右是属性（1）还是值（0）
	AttrType attrType;
	int LattrLength,RattrLength;
	int LattrOffset,RattrOffset;
	CompOp compOp;
	void *Lvalue,*Rvalue;
}Con;



typedef struct{
	bool  bOpen;		//扫描是否打开 
	RM_FileHandle  *pRMFileHandle;		//扫描的记录文件句柄
	int  conNum;		//扫描涉及的条件数量 
	Con  *conditions;	//扫描涉及的条件数组指针
    PF_PageHandle  PageHandle; //处理中的页面句柄
	PageNum  pn; 	//扫描即将处理的页面号
	SlotNum  sn;		//扫描即将处理的插槽号
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
void ConfigRMFilePageHandle(RM_PageHandle* rmPageHandle, RM_FileHandle* rmFileHandle, PF_PageHandle* pfPageHandle);
int RM_BitCount(char* bitmap, int size);
RC RM_GetThisPage(RM_FileHandle* rmFileHandle, PageNum pageNum, RM_PageHandle* rmPageHandle);
RC RM_AllocatePage(RM_FileHandle* fileHandle, RM_PageHandle* rmPageHandle);
bool equalStr(char* str0, char* str1, int len);

#endif