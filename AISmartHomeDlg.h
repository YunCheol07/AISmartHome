// AISmartHomeDlg.h: 헤더 파일

#pragma once

#include <afxwin.h>       
#include <afxsock.h>
#include "mysql.h"
#include "rapidjson/document.h"
#include <string>
#include <vector>
#include "SerialClass.h"
#include <afxinet.h>  

#pragma comment(lib,"libmysql.lib")
#pragma comment(lib,"ws2_32.lib")

// Database connection information
#define DB_ADDRESS "127.0.0.0"
#define DB_ID "root"
#define DB_PASS "123412412"
#define DB_NAME "smarthome1"
#define DB_PORT 3306 

struct WeatherInfo { // API 외부날씨 데이터
	CString sky;  // 날씨 상태
	std::string tmp;  // 온도
	CString pty;  // 강수 형태
	std::string pop;  // 강수 확률 
	std::string reh;  // 습도
};

struct MicroInfo { // API 미세먼지 데이터
	std::string Micro; // 미세먼지 수치
};

struct UVInfo { // API 자외선 데이터
	CString code;
	CString areaNo;
	CString date;
	std::string UVdata;  // 시간별 UV 수치
};

class CMySocket : public CAsyncSocket
{
public:
	int clientID;               // 고유 클라이언트 ID
	bool m_bReadyToReceive;     // 데이터 수신 준비 상태

	CMySocket();                // 생성자 선언
	virtual ~CMySocket();       // 소멸자 선언

	// 소켓 이벤트 핸들러
	virtual void OnReceive(int nErrorCode);
	virtual void OnClose(int nErrorCode);
	virtual void OnAccept(int nErrorCode);

	// 연결 상태 확인 메서드
	bool IsConnected();
};

// CAISmartHomeDlg 대화 상자
class CAISmartHomeDlg : public CDialogEx
{
// 생성입니다.
public:
	CAISmartHomeDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.
	virtual ~CAISmartHomeDlg(); //소멸자

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AISMARTHOME_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.

public:

	// Constants for weather update timing
	static const UINT DATA_UPDATE_TIMER = 1;
	//static const UINT UPDATE_INTERVAL = 300000; // 10 minutes in milliseconds
	//static const UINT UPDATE_INTERVAL = 180000; // 3 minutes in milliseconds
	static const UINT UPDATE_INTERVAL = 5000; // 5 sec

	// DB 연결 변수
	MYSQL* Connection;
	MYSQL_RES* SQL_result;
	MYSQL_ROW Row;
	int query_state;

	// 서버, 클라이언트 소켓
	CMySocket m_serverSocket;
	CMySocket m_clientSocket;


public: // 외부 날씨 데이터
	// 날씨 정보 변수
	CString SKY_D;
	CString TEMP_D;
	CString PTY_D;
	CString POP_D;
	CString REH_D;
	CString UV_D;
	CString MICRO_D;
	bool Outtemp_ready = false; // 외부온도 준비 변수 (DB 전송에 필요)
	// 날씨 데이터 받아오고 파싱하는 함수
	WeatherInfo ParseWeatherData(const CString& jsonStr);
	CString GetWeatherData(void);
	MicroInfo ParseMicroData(const CString& jsonStr);
	CString GetMicroData(void);
	UVInfo ParseUVData(const CString& jsonStr);
	CString GetUVData(void);

public: // 소켓통신
	CString UTF8ToCString(const std::string& utf8Str); // 인코딩 형식 변환 함수
	void Update(); // 데이터 갱신 함수 (외부날씨, 센서, 딥러닝)
	void WeatherHandle(CMySocket* pClientSocket); // 외부날씨 클라이언트 전송 처리 함수
	void LoginHandle(CMySocket* pSenderSocket, const CString& loginMessage); // 로그인 클라이언트 전송 처리 함수
	void SendToClient(CMySocket* pClientSocket, const std::vector<char>& utf8Data, const CString& label, int clientID); // 데이터 클라이언트 전송 함수
	void RegisterHandle(CMySocket* pSenderSocket, const CString& registerMessage); // 회원가입 클라이언트 전송 처리 함수
	void MainHandle(CMySocket* pClientSocket); // 클라이언트 메인에 필요한 데이터 전송 처리 함수
	void ValidateUserHandle(CMySocket* pSenderSocket, const CString& username); // 회원가입 중복 확인 클라이언트 전송 처리 함수
	void UsersetTEMPHandle(CMySocket* pSenderSocket, const CString& username); // 사용자 설정 온도 클라이언트 전송 처리 함수

public:	// 클라이언트 소켓 크리티컬 섹션 처리 
	void LockClientSockets(); 
	void UnlockClientSockets();
	int GenerateUniqueClientID(); // 클라이언트 아이디 지정
	CArray<CMySocket*, CMySocket*> m_ClientSockets; // 클라이언트 소켓 리스트
	CRITICAL_SECTION m_ClientSocketsLock;            // 동기화용 크리티컬 섹션

public: // 파이썬 서버 연결
	void RecommendTEMPON(); // 추천온도 ON 작동 함수
	void RecommendTEMPOFF(); // 추천온도 OFF 작동 함수
	void SendWeatherDataToPython(CMySocket* pClientSocket, const WeatherInfo& info, const MicroInfo& Minfo, const UVInfo& UVinfo); // 팁 구현을 위해 외부날씨 파이썬 서버로 전송하는 함수
	double m_outTEMP; // 외부 온도 저장 변수
	double m_inTEMP;  // 내부 온도 저장 변수
	// double inTEMP; //아두이노 온도 센서 데이터 (건드릴꺼 없음) 
	bool ConnectToServer(); // 파이썬 서버 연결 확인 함수
	void DisconnectFromServer(); // 파이썬 서버 연결 종료 함수
	void SendTrainModelRequest(); // 딥러닝 모델 요청 함수
	double SendDataToPythonServer(double num1, double num2); // 추천온도에 필요한 데이터 파이썬 서버로 전송하는 함수
	void TrainModel(); // 딥러닝 실행 함수
	void SendAdviceToAndroid(const CString& adviceMessage); // GPT 팁 전송

private: // 파이썬 서버 연결
	CInternetSession m_session;
	CHttpConnection* pServer = nullptr;

public: // 시리얼 센서 데이터
	SerialClass m_serial;  // 시리얼 통신을 위한 클래스 인스턴스
	void SerialData(); // 시리얼 데이터 받아오는 함수
	void ParseSerialData(const CString& strLine); // 시리얼 데이터 파싱 함수
	float g_temp = 0, g_hum = 0, g_dust = 0; // 센서 저장 전역 변수
	bool temp_ready = false, hum_ready = false, dust_ready = false; // 센서 준비 여부 변수
	bool btnState = false; // 버튼 상태 여부 변수
	bool AllDataReady(); // DB로 센서 데이터 전송 전 데이터 확인 함수
	void ResetData(); // 데이터 초기화 함수
	void SensorDataHandle(CMySocket* pClientSocket); // 클라이언트 전송 센서 데이터 처리 함수
	void SaveToDatabase(); // DB에 저장하는 센서 데이터 함수
	void SendButtonState(bool state); // 버튼 상태를 클라이언트에게 전송하는 함수

public: // 센서 제어 함수
	void SendPFANON(); // 공기청정기 FAN ON
	void SendPFANOFF(); // 공기청정기 FAN OFF
	void SendACON(); // 에어컨 FAN ON
	void SendACOFF(); // 에어컨 FAN OFF
	void SendHFANON(); // 히터 FAN ON
	void SendHFANOFF(); // 히터 FAN OFF
	void SendLEDON(); // LED ON
	void SendLEDOFF(); // LED OFF
	void SendUSERTEMPOFF(); // 사용자 설정 온도 OFF

private:
	void HandleException(CException* e); //예외처리

public: // 로그인 대기 함수 (로그인 에러 방지)
	bool m_bWaitingForLoginData; 
	void SetWaitingForLoginData(bool isWaiting); 
	bool IsWaitingForLoginData() const;

// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};
