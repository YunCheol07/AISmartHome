#include "pch.h"
#include "SerialClass.h"

// 생성자
SerialClass::SerialClass() : m_hIDComDev(NULL) {
    // OVERLAPPED 구조체 초기화
    memset(&m_OverlappedRead, 0, sizeof(OVERLAPPED));
    memset(&m_OverlappedWrite, 0, sizeof(OVERLAPPED));
    // 비동기 I/O 작업을 위한 이벤트 객체 생성
    m_OverlappedRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_OverlappedWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

// 소멸자
SerialClass::~SerialClass() {
    Close(); // 시리얼 포트 닫기 함수 호출
}

// 시리얼 포트 열기
BOOL SerialClass::Open(int nPort, int nBaud) {
    DCB dcb; // 디바이스 제어 블록 구조체
    CString strPort; // 포트 이름을 저장할 CString 객체
    strPort.Format(_T("COM%d"), nPort); // 포트 이름 포맷 설정
    // 시리얼 포트를 비동기 모드로 열기
    m_hIDComDev = CreateFile(strPort, GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    if (m_hIDComDev == INVALID_HANDLE_VALUE) {
        // 포트 열기 실패 시 에러 메시지 출력
        DWORD dwError = GetLastError();
        CString strError;
        //strError.Format(_T("Failed to open serial port COM%d (Error Code: %ld)"), nPort, dwError);
        AfxMessageBox(strError);
        return FALSE;
    }
    // DCB 구조체 설정
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
    // 통신 설정 적용
    if (!SetCommState(m_hIDComDev, &dcb) || !SetupComm(m_hIDComDev, 1024, 1024)) {
        CloseHandle(m_hIDComDev);
        m_hIDComDev = NULL;
        //AfxMessageBox(_T("Failed to configure serial port settings"));
        return FALSE;
    }

    return TRUE;
}

// 시리얼 포트 닫기
BOOL SerialClass::Close() {
    if (m_hIDComDev != NULL) {
        CloseHandle(m_hIDComDev); // 핸들 닫기
        m_hIDComDev = NULL; // 핸들 초기화
    }
    return TRUE;
}

// 데이터 읽기
int SerialClass::ReadData(void* buffer, int limit) {
    if (m_hIDComDev == NULL) return 0; // 디바이스 핸들 검사
    BOOL bReadStatus;
    DWORD dwBytesRead, dwErrorFlags;
    COMSTAT comStat;

    ClearCommError(m_hIDComDev, &dwErrorFlags, &comStat); // 통신 오류 및 상태 정보 얻기
    if (dwErrorFlags > 0) { // 통신 오류가 있을 경우 오류 메시지 출력
        CString strError;
        //strError.Format(_T("Comm error: %ld"), dwErrorFlags);
        AfxMessageBox(strError);
    }

    if (!comStat.cbInQue) return 0; // 읽을 데이터가 없다면 0 반환

    // 데이터 읽기 요청
    bReadStatus = ReadFile(m_hIDComDev, buffer, min((DWORD)limit, comStat.cbInQue), &dwBytesRead, &m_OverlappedRead);
    if (!bReadStatus) { // 읽기 실패 시 처리
        if (GetLastError() == ERROR_IO_PENDING) { // IO 작업이 계속 진행 중이면
            if (WaitForSingleObject(m_OverlappedRead.hEvent, 2000) == WAIT_OBJECT_0) {
                GetOverlappedResult(m_hIDComDev, &m_OverlappedRead, &dwBytesRead, FALSE);
                return (int)dwBytesRead;
            }
            else { // 타임아웃 발생
                //AfxMessageBox(_T("Read operation timed out."));
                return 0;
            }
        }
        // 기타 오류 처리
        CString strError;
        DWORD dwError = GetLastError();
        //strError.Format(_T("Read failed (Error Code: %ld)"), dwError);
        AfxMessageBox(strError);
        return 0;
    }
    return (int)dwBytesRead; // 읽은 바이트 수 반환
}

// 데이터를 시리얼 포트로 전송하는 함수
BOOL SerialClass::WriteData(const void* buffer, int size)
{
    if (m_hIDComDev == NULL) return FALSE; // 시리얼 포트가 열려 있는지 확인

    BOOL bWriteStatus;
    DWORD dwBytesWritten;

    // 비동기 쓰기 요청
    bWriteStatus = WriteFile(m_hIDComDev, buffer, size, &dwBytesWritten, &m_OverlappedWrite);

    if (!bWriteStatus) {
        if (GetLastError() == ERROR_IO_PENDING) {
            // 쓰기 작업이 비동기적으로 진행되고 있을 경우
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

    return TRUE; // 성공적으로 데이터를 전송했을 경우 TRUE 반환
}