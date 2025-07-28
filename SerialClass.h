#pragma once
#include <afxdialogex.h>
class SerialClass : public CDialogEx
{
public:
    SerialClass();             // ������
    virtual ~SerialClass();    // �Ҹ���
    BOOL Open(int nPort = 3, int nBaud = 9600);  // �ø��� ��Ʈ�� ���� ���� �Լ�
    BOOL Close();              // �ø��� ��Ʈ�� �ݱ� ���� �Լ�
    int ReadData(void* buffer, int limit);  // �����͸� �б� ���� �Լ�
    BOOL WriteData(const void* buffer, int size);
protected:
    HANDLE m_hIDComDev;        // �ø��� ��� �ڵ�
    OVERLAPPED m_OverlappedRead, m_OverlappedWrite;  // �񵿱� ����� ���� ����ü
};

