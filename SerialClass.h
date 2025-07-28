#pragma once
#include <afxdialogex.h>
class SerialClass : public CDialogEx
{
public:
    SerialClass();             // 생성자
    virtual ~SerialClass();    // 소멸자
    BOOL Open(int nPort = 3, int nBaud = 9600);  // 시리얼 포트를 열기 위한 함수
    BOOL Close();              // 시리얼 포트를 닫기 위한 함수
    int ReadData(void* buffer, int limit);  // 데이터를 읽기 위한 함수
    BOOL WriteData(const void* buffer, int size);
protected:
    HANDLE m_hIDComDev;        // 시리얼 통신 핸들
    OVERLAPPED m_OverlappedRead, m_OverlappedWrite;  // 비동기 통신을 위한 구조체
};

