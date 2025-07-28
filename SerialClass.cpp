#include "pch.h"
#include "SerialClass.h"

// ������
SerialClass::SerialClass() : m_hIDComDev(NULL) {
    // OVERLAPPED ����ü �ʱ�ȭ
    memset(&m_OverlappedRead, 0, sizeof(OVERLAPPED));
    memset(&m_OverlappedWrite, 0, sizeof(OVERLAPPED));
    // �񵿱� I/O �۾��� ���� �̺�Ʈ ��ü ����
    m_OverlappedRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_OverlappedWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

// �Ҹ���
SerialClass::~SerialClass() {
    Close(); // �ø��� ��Ʈ �ݱ� �Լ� ȣ��
}

// �ø��� ��Ʈ ����
BOOL SerialClass::Open(int nPort, int nBaud) {
    DCB dcb; // ����̽� ���� ��� ����ü
    CString strPort; // ��Ʈ �̸��� ������ CString ��ü
    strPort.Format(_T("COM%d"), nPort); // ��Ʈ �̸� ���� ����
    // �ø��� ��Ʈ�� �񵿱� ���� ����
    m_hIDComDev = CreateFile(strPort, GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    if (m_hIDComDev == INVALID_HANDLE_VALUE) {
        // ��Ʈ ���� ���� �� ���� �޽��� ���
        DWORD dwError = GetLastError();
        CString strError;
        //strError.Format(_T("Failed to open serial port COM%d (Error Code: %ld)"), nPort, dwError);
        AfxMessageBox(strError);
        return FALSE;
    }
    // DCB ����ü ����
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    dcb.BaudRate = nBaud;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fAbortOnError = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    // ��� ���� ����
    if (!SetCommState(m_hIDComDev, &dcb) || !SetupComm(m_hIDComDev, 1024, 1024)) {
        CloseHandle(m_hIDComDev);
        m_hIDComDev = NULL;
        //AfxMessageBox(_T("Failed to configure serial port settings"));
        return FALSE;
    }

    return TRUE;
}

// �ø��� ��Ʈ �ݱ�
BOOL SerialClass::Close() {
    if (m_hIDComDev != NULL) {
        CloseHandle(m_hIDComDev); // �ڵ� �ݱ�
        m_hIDComDev = NULL; // �ڵ� �ʱ�ȭ
    }
    return TRUE;
}

// ������ �б�
int SerialClass::ReadData(void* buffer, int limit) {
    if (m_hIDComDev == NULL) return 0; // ����̽� �ڵ� �˻�
    BOOL bReadStatus;
    DWORD dwBytesRead, dwErrorFlags;
    COMSTAT comStat;

    ClearCommError(m_hIDComDev, &dwErrorFlags, &comStat); // ��� ���� �� ���� ���� ���
    if (dwErrorFlags > 0) { // ��� ������ ���� ��� ���� �޽��� ���
        CString strError;
        //strError.Format(_T("Comm error: %ld"), dwErrorFlags);
        AfxMessageBox(strError);
    }

    if (!comStat.cbInQue) return 0; // ���� �����Ͱ� ���ٸ� 0 ��ȯ

    // ������ �б� ��û
    bReadStatus = ReadFile(m_hIDComDev, buffer, min((DWORD)limit, comStat.cbInQue), &dwBytesRead, &m_OverlappedRead);
    if (!bReadStatus) { // �б� ���� �� ó��
        if (GetLastError() == ERROR_IO_PENDING) { // IO �۾��� ��� ���� ���̸�
            if (WaitForSingleObject(m_OverlappedRead.hEvent, 2000) == WAIT_OBJECT_0) {
                GetOverlappedResult(m_hIDComDev, &m_OverlappedRead, &dwBytesRead, FALSE);
                return (int)dwBytesRead;
            }
            else { // Ÿ�Ӿƿ� �߻�
                //AfxMessageBox(_T("Read operation timed out."));
                return 0;
            }
        }
        // ��Ÿ ���� ó��
        CString strError;
        DWORD dwError = GetLastError();
        //strError.Format(_T("Read failed (Error Code: %ld)"), dwError);
        AfxMessageBox(strError);
        return 0;
    }
    return (int)dwBytesRead; // ���� ����Ʈ �� ��ȯ
}

// �����͸� �ø��� ��Ʈ�� �����ϴ� �Լ�
BOOL SerialClass::WriteData(const void* buffer, int size)
{
    if (m_hIDComDev == NULL) return FALSE; // �ø��� ��Ʈ�� ���� �ִ��� Ȯ��

    BOOL bWriteStatus;
    DWORD dwBytesWritten;

    // �񵿱� ���� ��û
    bWriteStatus = WriteFile(m_hIDComDev, buffer, size, &dwBytesWritten, &m_OverlappedWrite);

    if (!bWriteStatus) {
        if (GetLastError() == ERROR_IO_PENDING) {
            // ���� �۾��� �񵿱������� ����ǰ� ���� ���
            if (WaitForSingleObject(m_OverlappedWrite.hEvent, 2000) == WAIT_OBJECT_0) {
                GetOverlappedResult(m_hIDComDev, &m_OverlappedWrite, &dwBytesWritten, FALSE);
                return TRUE;
            }
            else {
                AfxMessageBox(_T("Write operation timed out."));
                return FALSE;
            }
        }
        CString strError;
        DWORD dwError = GetLastError();
        strError.Format(_T("Write failed (Error Code: %ld)"), dwError);
        AfxMessageBox(strError);
        return FALSE;
    }

    return TRUE; // ���������� �����͸� �������� ��� TRUE ��ȯ
}