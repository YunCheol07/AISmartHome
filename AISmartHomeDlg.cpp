// AISmartHomeDlg.cpp: 구현 파일

#include "pch.h"
#include "framework.h"
#include "AISmartHome.h"
#include "AISmartHomeDlg.h"
#include "afxdialogex.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h" 
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <afxinet.h>
#include <string>
#include <vector>
#include <map>
#include <afxsock.h>
#include <future>
#include <iostream>
#include <atlstr.h> 
#include <chrono>
#include <thread>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

// 구현입니다.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CAISmartHomeDlg 대화 상자



CAISmartHomeDlg::CAISmartHomeDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_AISMARTHOME_DIALOG, pParent), m_bWaitingForLoginData(false) // 플래그 초기화
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	InitializeCriticalSection(&m_ClientSocketsLock); // 뮤텍스 초기화
	m_bWaitingForLoginData = false;
}

void CAISmartHomeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAISmartHomeDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CAISmartHomeDlg 메시지 처리기

BOOL CAISmartHomeDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// TODO: 여기에 추가 초기화 작업을 추가합니다.

	// 타이머 시작: 10분마다 SHOW() 함수 호출
	SetTimer(DATA_UPDATE_TIMER, UPDATE_INTERVAL, NULL);
	CString jsonStr = GetWeatherData();
	Update(); // 함수 호출
	m_serial.Open(3, 9600);  // COM3 포트를 9600bps로 열기
	// DB 연결 설정
	Connection = mysql_init(NULL);
	if (!mysql_real_connect(Connection, DB_ADDRESS, DB_ID, DB_PASS, DB_NAME, DB_PORT, NULL, 0))
	{
		AfxMessageBox(mysql_error(Connection), MB_OK | MB_ICONERROR);
	}
	else
	{
		//AfxMessageBox(_T("DB 연결 성공"), MB_OK | MB_ICONINFORMATION);
		OutputDebugString(_T("DB connect OK\n")); 
	}

	// 서버 소켓 설정 및 리스닝 시작
	if (m_serverSocket.Create(12345, SOCK_STREAM))
	{
		if (!m_serverSocket.Listen())
		{
			//AfxMessageBox(_T("Failed to listen on port 12345"), MB_OK | MB_ICONERROR);
			OutputDebugString(_T("Failed to listen on port 12345\n"));
		}
	}
	else
	{
		//AfxMessageBox(_T("Failed to create server socket"), MB_OK | MB_ICONERROR);
		OutputDebugString(_T("Failed to create server socket\n"));
	}

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void CAISmartHomeDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CAISmartHomeDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CAISmartHomeDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// 추가 CString을 UTF-8로 정확히 변환하는 헬퍼 함수
std::vector<char> CStringToUTF8Vector(const CString& str)
{
	CStringW unicodeStr(str);
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, unicodeStr, -1, NULL, 0, NULL, NULL);
	if (size_needed <= 0)
		return {};

	std::vector<char> utf8Str(size_needed - 1); // 널 종료 문자를 제외
	WideCharToMultiByte(CP_UTF8, 0, unicodeStr, -1, utf8Str.data(), size_needed - 1, NULL, NULL);
	return utf8Str;
}

CAISmartHomeDlg::~CAISmartHomeDlg()
{
	for (int i = 0; i < m_ClientSockets.GetSize(); ++i)
	{
		CMySocket* pClientSocket = m_ClientSockets[i];
		if (pClientSocket != nullptr)
		{
			pClientSocket->Close();
			delete pClientSocket;
		}
	}
	m_ClientSockets.RemoveAll();
	DeleteCriticalSection(&m_ClientSocketsLock); // 뮤텍스 삭제
}

// 시리얼 데이터 읽고 변수로 변환하는 함수
void CAISmartHomeDlg::SerialData()
{
	static CString strBuffer;  // 데이터를 임시로 저장할 공간을 만듦 (버퍼)
	char szTemp[1024];  // 데이터를 잠시 저장할 임시 공간
	int nRead = m_serial.ReadData(szTemp, sizeof(szTemp) - 1);  // 시리얼 포트에서 데이터를 읽음

	if (nRead > 0) {  // 읽은 데이터가 있으면
		szTemp[nRead] = '\0';  // 문자열 끝을 표시하는 null 문자 추가
		strBuffer += szTemp;  // 읽은 데이터를 버퍼에 추가

		// 버퍼에서 줄 단위로 데이터를 처리
		int nIndex = strBuffer.Find('\n');  // 줄 끝(엔터)의 위치를 찾음
		while (nIndex != -1) {  // 줄 끝이 있으면
			CString strLine = strBuffer.Left(nIndex);  // 줄 끝까지의 데이터를 가져옴
			strBuffer = strBuffer.Mid(nIndex + 1);  // 나머지 데이터를 버퍼에 남겨둠

			ParseSerialData(strLine);  // 가져온 데이터를 파싱(해석)해서 센서 값으로 변환

			nIndex = strBuffer.Find('\n');  // 다음 줄 끝의 위치를 찾음
		}
	}
}

// 센서 데이터를 해석해서 각각의 센서 값으로 변환하는 함수
void CAISmartHomeDlg::ParseSerialData(const CString& strLine)
{
	// \r 문자를 제거하여 깨끗한 문자열을 만듦
	CString cleanLine = strLine;
	cleanLine.TrimRight(_T("\r")); // 캐리지 리턴 제거

	float temp = 0, hum = 0, dust = 0;

	CString strtemp, strhum, strdust;

	// 온도 데이터를 읽고 처리
	if (sscanf_s(strLine, "temperature: %f", &temp) == 1) {
		g_temp = temp;
		strtemp.Format(_T("%.2f"), temp);
		temp_ready = true;
		OutputDebugString(_T("Parsed Temperature\n"));
	}
	// 습도 데이터를 읽고 처리
	else if (sscanf_s(strLine, "humidity: %f", &hum) == 1) {
		g_hum = hum;
		strhum.Format(_T("%.2f"), hum);
		hum_ready = true;
		OutputDebugString(_T("Parsed humidity\n"));
	}

	// 미세먼지 데이터를 읽고 처리
	else if (sscanf_s(strLine, "Dust Density [ug/m3]: %f", &dust) == 1) {
		g_dust = dust;
		strdust.Format(_T("%.2f"), dust);
		dust_ready = true;
		OutputDebugString(_T("Parsed Dust Density\n"));
	}
	// 버튼 상태 데이터를 읽고 처리
	else if (cleanLine == "BTN_ON") {
		btnState = true;
		OutputDebugString(_T("Button Pressed: BTN_ON\n"));
		// 버튼 상태 전송 함수 호출
		SendButtonState(btnState);
	}
	else if (cleanLine == "BTN_OFF") {
		btnState = false;
		OutputDebugString(_T("Button Released: BTN_OFF\n"));
	}

	m_inTEMP = g_temp; // 내부온도 저장 (파이썬 전달용)

}

// 모든 센서 데이터가 준비되었는지 확인하는 함수 
bool CAISmartHomeDlg::AllDataReady() {
	// 모든 센서 데이터가 준비되었는지 여부를 확인
	return Outtemp_ready && temp_ready && hum_ready && dust_ready;
}

// 센서 데이터의 준비 상태를 초기화하는 함수 
void CAISmartHomeDlg::ResetData() {
	// 모든 준비 상태를 초기화
	Outtemp_ready = temp_ready = hum_ready = dust_ready = false;
}

// 센서 데이터를 데이터베이스에 저장하는 함수
void CAISmartHomeDlg::SaveToDatabase() {
	// 데이터베이스에 넣을 명령문을 만듦
	CString query;
	query.Format(_T("INSERT INTO deeplearning (outtemp, temp, humi, dust) VALUES ('%.2f', '%.2f', '%.2f', '%.2f')"),
		m_outTEMP, g_temp, g_hum, g_dust);

	// 데이터베이스에 명령문을 보내서 데이터를 저장
	if (mysql_query(Connection, CT2A(query))) {
		// 만약 에러가 발생했다면 에러 메시지를 보여줌
		OutputDebugString(_T("query error\n"));
	}
	else {
		// 데이터가 잘 저장되었다는 메시지를 보여줄 수 있음
		OutputDebugString(_T("query success\n"));
		// 데이터가 저장된 후 상태를 초기화
		ResetData();
	}
}

// 웹 API에서 날씨 데이터를 가져오는 함수
CString CAISmartHomeDlg::GetWeatherData(void)
{
	// 현재 시간을 가져옵니다
	CTime currentTime = CTime::GetCurrentTime();
	int hour = currentTime.GetHour();
	CString base_date, base_time;

	// 현재 시간에 따라 날씨 데이터를 요청할 시간(base_time)을 설정합니다
	if (hour < 2) {
		base_time = _T("2300");  // 새벽 2시 이전이라면, 전날 밤 11시의 데이터를 요청
		currentTime -= CTimeSpan(1, 0, 0, 0);  // 날짜를 하루 전으로 변경
	}
	else if (hour < 5) base_time = _T("0200");  // 새벽 2시 이후라면 2시의 데이터를 요청
	else if (hour < 8) base_time = _T("0500");  // 새벽 5시 이후라면 5시의 데이터를 요청
	else if (hour < 11) base_time = _T("0800");  // 오전 8시 이후라면 8시의 데이터를 요청
	else if (hour < 14) base_time = _T("1100");  // 오전 11시 이후라면 11시의 데이터를 요청
	else if (hour < 17) base_time = _T("1400");  // 오후 2시 이후라면 2시의 데이터를 요청
	else if (hour < 20) base_time = _T("1700");  // 오후 5시 이후라면 5시의 데이터를 요청
	else if (hour < 23) base_time = _T("2000");  // 오후 8시 이후라면 8시의 데이터를 요청
	else base_time = _T("2300");  // 밤 11시 이후라면 11시의 데이터를 요청

	// 현재 날짜를 가져옵니다
	base_date = currentTime.Format(_T("%Y%m%d"));

	// API 요청을 보낼 URL을 만듭니다
	CString strServer = _T("apis.data.go.kr");
	CString strObject = _T("/1360000/VilageFcstInfoService_2.0/getVilageFcst?serviceKey=IX00uD9E9adAFKgHHZPdmZWNgt0ostdQXrboQz97Rhf96cWCdfdoKdlZxnTRsMuauxI3csG5PqXYZbFDh1LsPQ%3D%3D&dataType=JSON&numOfRows=12&pageNo=1&base_date=") + base_date + _T("&base_time=") + base_time + _T("&nx=59&ny=110");
	CString strAgent = _T("MFC App");
	CInternetSession session(strAgent);
	CString strResponse;

	try {
		// 서버에 연결하여 요청을 보냅니다
		CHttpConnection* pConnection = session.GetHttpConnection(strServer, (INTERNET_PORT)80, NULL, NULL);
		CHttpFile* pFile = pConnection->OpenRequest(CHttpConnection::HTTP_VERB_GET, strObject);

		// 서버에서 응답을 받습니다
		pFile->SendRequest();
		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK) {
			// 응답이 성공적이면 데이터를 읽어옵니다
			CString strTemp;
			while (pFile->ReadString(strTemp))
				strResponse += strTemp;
		}
		else {
			// 응답이 실패하면 상태 코드를 표시합니다
			CString errorMsg;
			errorMsg.Format(_T("HTTP 요청 실패: 상태 코드 %lu"), dwStatusCode);
			AfxMessageBox(errorMsg);
		}
		// 연결을 닫습니다
		pFile->Close();
		delete pFile;
		pConnection->Close();
		delete pConnection;
	}

	// 에러가 발생하면 예외를 처리합니다
	catch (CInternetException* e) {
		e->Delete();
	}

	// 세션을 닫습니다
	session.Close();

	// 서버로부터 받은 데이터를 반환합니다
	return strResponse;
}

// JSON 데이터를 파싱하여 날씨 정보를 구조체에 저장하는 함수
WeatherInfo CAISmartHomeDlg::ParseWeatherData(const CString& jsonStr)
{
	WeatherInfo info;  // 날씨 정보를 저장할 구조체
	CT2CA pszConvertedAnsiString(jsonStr, CP_UTF8);  // CString을 표준 문자열로 변환
	std::string strStd(pszConvertedAnsiString);

	rapidjson::Document doc;  // JSON 데이터를 파싱할 객체
	doc.Parse(strStd.c_str());

	// JSON 데이터 파싱 중 에러가 발생했는지 확인합니다
	if (doc.HasParseError()) {
		return info;  // 에러가 발생하면 빈 구조체를 반환
	}

	// JSON 데이터에서 날씨 정보를 추출하여 구조체에 저장합니다
	const auto& items = doc["response"]["body"]["items"]["item"].GetArray();
	for (const auto& item : items) {
		std::string category = item["category"].GetString();  // 카테고리를 가져옴
		std::string value = item["fcstValue"].GetString();  // 예보 값을 가져옴

		// 카테고리에 따라 해당 값을 구조체에 저장
		if (category == "SKY") {
			if (value == "1") info.sky = _T("맑음");
			else if (value == "3") info.sky = _T("구름많음");
			else if (value == "4") info.sky = _T("흐림");
		}
		else if (category == "TMP") info.tmp = UTF8ToCString(value);  // 온도
		else if (category == "PTY") {  // 강수 형태
			if (value == "0") info.pty = _T(" ");
			else if (value == "1") info.pty = _T("비");
			else if (value == "2") info.pty = _T("진눈깨비");
			else if (value == "3") info.pty = _T("눈");
			else if (value == "4") info.pty = _T("소나기");
		}
		else if (category == "POP") info.pop = UTF8ToCString(value);  // 강수 확률
		else if (category == "REH") info.reh = UTF8ToCString(value);  // 습도

	}

	SKY_D = CString(info.sky);
	TEMP_D = CString(info.tmp.c_str());
	PTY_D = CString(info.pty);
	POP_D = CString(info.pop.c_str());
	REH_D = CString(info.reh.c_str());

	m_outTEMP = _tstof(TEMP_D);
	Outtemp_ready = true; // DB 저장용

	OutputDebugString(_T("Parsed OUT Weather\n")); // 외부날씨 파싱 여부 확인 로그

	return info;  // 파싱된 날씨 정보를 반환
}

// 웹 API에서 미세먼지 데이터를 가져오는 함수
CString CAISmartHomeDlg::GetMicroData(void)
{
	//%EB%8F%84%EA%B3%A0%EB%A9%B4 = 도고면(미세먼지 측정소)
	// API 요청을 보낼 URL을 만듭니다
	CString strServer = _T("apis.data.go.kr");
	CString strObject = _T("/B552584/ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty?serviceKey=IX00uD9E9adAFKgHHZPdmZWNgt0ostdQXrboQz97Rhf96cWCdfdoKdlZxnTRsMuauxI3csG5PqXYZbFDh1LsPQ%3D%3D&returnType=json&numOfRows=1&pageNo=1&stationName=%EB%8F%84%EA%B3%A0%EB%A9%B4&dataTerm=DAILY&ver=1.2");
	CString strAgent = _T("MFC App");
	CInternetSession session(strAgent);
	CString MicroResponse;

	try {
		// 서버에 연결하여 요청을 보냅니다
		CHttpConnection* pConnection = session.GetHttpConnection(strServer, (INTERNET_PORT)80, NULL, NULL);
		CHttpFile* pFile = pConnection->OpenRequest(CHttpConnection::HTTP_VERB_GET, strObject);

		// 서버에서 응답을 받습니다
		pFile->SendRequest();
		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK) {
			// 응답이 성공적이면 데이터를 읽어옵니다
			CString strTemp;
			while (pFile->ReadString(strTemp))
				MicroResponse += strTemp;
		}
		else {
			// 응답이 실패하면 상태 코드를 표시합니다
			CString errorMsg;
			errorMsg.Format(_T("HTTP 요청 실패: 상태 코드 %lu"), dwStatusCode);
			AfxMessageBox(errorMsg);
		}
		// 연결을 닫습니다
		pFile->Close();
		delete pFile;
		pConnection->Close();
		delete pConnection;
	}

	// 에러가 발생하면 예외를 처리합니다
	catch (CInternetException* e) {
		e->Delete();
	}

	// 세션을 닫습니다
	session.Close();

	// 서버로부터 받은 데이터를 반환합니다
	return MicroResponse;
}

// JSON 데이터를 파싱하여 미세먼지 정보를 구조체에 저장하는 함수
MicroInfo CAISmartHomeDlg::ParseMicroData(const CString& jsonStr)
{
	MicroInfo Minfo;  // 미세먼지 정보를 저장할 구조체
	CT2CA pszConvertedAnsiString(jsonStr, CP_UTF8);  // CString을 표준 문자열로 변환
	std::string strStd(pszConvertedAnsiString);

	rapidjson::Document doc;  // JSON 데이터를 파싱할 객체
	doc.Parse(strStd.c_str());

	// JSON 데이터 파싱 중 에러가 발생했는지 확인합니다
	if (doc.HasParseError()) {
		// 파싱 오류 정보를 출력
		CString errorMsg;
		return Minfo;  // 에러가 발생하면 빈 구조체를 반환
	}

	// JSON 데이터에서 미세먼지(pm10Value) 정보를 추출하여 구조체에 저장합니다
	if (doc.HasMember("response") && doc["response"].HasMember("body") && doc["response"]["body"].HasMember("items")) {
		const auto& items = doc["response"]["body"]["items"].GetArray();
		for (const auto& item : items) {
			if (item.HasMember("pm10Value")) {
				std::string pm10Value = item["pm10Value"].GetString();
				Minfo.Micro = pm10Value;  // 미세먼지 값을 구조체에 저장
				// 파싱된 데이터를 디버깅 로그로 출력
				//TRACE("pm10Value: %s\n", Minfo.Micro.c_str());
			}
		}
	}
	else {
		AfxMessageBox(_T("JSON 구조가 예상과 다릅니다."));
	}

	// 날씨 정보를 CString으로 변환하여 멤버 변수에 저장합니다
	MICRO_D = CString(Minfo.Micro.c_str());

	OutputDebugString(_T("Parsed OUT Micro\n"));

	return Minfo;  // 파싱된 미세먼지 정보를 반환
}

// 웹 API에서 자외선 데이터를 가져오는 함수
CString CAISmartHomeDlg::GetUVData(void)
{
	// 현재 시간을 가져옵니다
	CTime currentTime = CTime::GetCurrentTime();
	CString timeParam = currentTime.Format(_T("%Y%m%d%H"));  // YYYYMMDDHH 형식으로 변환

	// API 요청을 보낼 URL을 만듭니다
	CString strServer = _T("apis.data.go.kr");
	CString strObject;
	strObject.Format(_T("/1360000/LivingWthrIdxServiceV4/getUVIdxV4?serviceKey=IX00uD9E9adAFKgHHZPdmZWNgt0ostdQXrboQz97Rhf96cWCdfdoKdlZxnTRsMuauxI3csG5PqXYZbFDh1LsPQ%%3D%%3D&pageNo=1&numOfRows=10&dataType=JSON&areaNo=4420041000&time=%s"), timeParam);
	CString strAgent = _T("MFC App");
	CInternetSession session(strAgent);
	CString strResponse;

	try {
		// 서버에 연결하여 요청을 보냅니다
		CHttpConnection* pConnection = session.GetHttpConnection(strServer, (INTERNET_PORT)80, NULL, NULL);
		CHttpFile* pFile = pConnection->OpenRequest(CHttpConnection::HTTP_VERB_GET, strObject);

		// 서버에서 응답을 받습니다
		pFile->SendRequest();
		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK) {
			// 응답이 성공적이면 데이터를 읽어옵니다
			CString strTemp;
			while (pFile->ReadString(strTemp))
				strResponse += strTemp;
		}
		else {
			// 응답이 실패하면 상태 코드를 표시합니다
			CString errorMsg;
			errorMsg.Format(_T("HTTP 요청 실패: 상태 코드 %lu"), dwStatusCode);
			AfxMessageBox(errorMsg);
		}
		// 연결을 닫습니다
		pFile->Close();
		delete pFile;
		pConnection->Close();
		delete pConnection;
	}

	// 에러가 발생하면 예외를 처리합니다
	catch (CInternetException* e) {
		e->Delete();
	}

	// 세션을 닫습니다
	session.Close();

	// 서버로부터 받은 데이터를 반환합니다
	return strResponse;
}

// JSON 데이터를 파싱하여 자외선 정보를 구조체에 저장하는 함수
UVInfo CAISmartHomeDlg::ParseUVData(const CString& jsonStr)
{
	UVInfo UVinfo;  // UV 정보를 저장할 구조체
	CT2CA pszConvertedAnsiString(jsonStr, CP_UTF8);  // CString을 표준 문자열로 변환
	std::string strStd(pszConvertedAnsiString);

	rapidjson::Document doc;  // JSON 데이터를 파싱할 객체
	doc.Parse(strStd.c_str());

	// JSON 데이터 파싱 중 에러가 발생했는지 확인합니다
	if (doc.HasParseError()) {
		AfxMessageBox(_T("JSON 파싱 실패"));
		return UVinfo;  // 에러가 발생하면 빈 구조체를 반환
	}

	// JSON 데이터에서 UV 정보를 추출하여 구조체에 저장합니다
	if (doc.HasMember("response") && doc["response"].HasMember("body") && doc["response"]["body"].HasMember("items") && doc["response"]["body"]["items"].HasMember("item")) {
		const auto& items = doc["response"]["body"]["items"]["item"].GetArray();
		for (const auto& item : items) {
			UVinfo.code = CString(item["code"].GetString());
			UVinfo.areaNo = CString(item["areaNo"].GetString());
			UVinfo.date = CString(item["date"].GetString());

			if (item.HasMember("h0")) {
				UVinfo.UVdata = item["h0"].GetString();  // h0 데이터를 문자열로 저장
			}
			else {
				UVinfo.UVdata = "N/A";  // h0 데이터가 없는 경우 "N/A"로 표시
			}
		}
	}
	else {
		AfxMessageBox(_T("JSON 구조가 예상과 다릅니다."));
	}

	UV_D = CString(UVinfo.UVdata.c_str());

	OutputDebugString(_T("Parsed UV\n"));

	return UVinfo;  // 파싱된 UV 정보를 반환
}

// 대화 상자에 날씨 정보를 표시하는 함수(데이터를 갱신하는 함수)
void CAISmartHomeDlg::Update()
{
	// 서버로부터 날씨 데이터를 가져옵니다
	CString strResponse = GetWeatherData();
	CString MicroResponse = GetMicroData();
	CString UVResponse = GetUVData();

	// 가져온 데이터를 파싱하여 구조체에 저장
	WeatherInfo info = ParseWeatherData(strResponse);
	MicroInfo Minfo = ParseMicroData(MicroResponse);
	UVInfo Uinfo = ParseUVData(UVResponse);

	SerialData();

	// 모든 센서 데이터가 준비되었는지 확인
	if (AllDataReady()) {
		SaveToDatabase(); // 데이터베이스에 저장
		ResetData(); // 데이터 전송 후 상태 초기화
	}

	/*
	// 1분 후에 SendWeatherDataToPython()과 RecommendTEMP(), SendTrainModelRequest() 실행
	std::thread delayedExecution([=]() {
		// 1분 대기
		std::this_thread::sleep_for(std::chrono::minutes(1));

		// 파이썬으로 날씨 데이터 전송
		//SendWeatherDataToPython(info, Minfo, Uinfo);

		// 추천 온도 계산
		//RecommendTEMP();

		// 딥러닝 학습 실행
		TrainModel();
		});

	// detached 상태로 실행: 메인 함수 종료 이후에도 백그라운드에서 실행됩니다.
	delayedExecution.detach();
	*/
}

// 데이터 갱신을 위한 타이머 함수
void CAISmartHomeDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == DATA_UPDATE_TIMER) {
		Update();  // 데이터를 갱신합니다
	}
	CDialogEx::OnTimer(nIDEvent);
}

// 생성자
CMySocket::CMySocket()
{
	m_bReadyToReceive = FALSE;
}

// 소멸자
CMySocket::~CMySocket()
{
	Close();
}

// 연결상태 체크
bool CMySocket::IsConnected()
{
	if (m_hSocket == INVALID_SOCKET)
		return false;

	// 피어 주소와 포트를 저장할 구조체 선언
	SOCKADDR_IN peerAddr;
	int addrLen = sizeof(peerAddr);
	memset(&peerAddr, 0, sizeof(peerAddr));

	// GetPeerName 호출
	if (GetPeerName((SOCKADDR*)&peerAddr, &addrLen))
	{
		// 피어 주소 추출 (선택 사항: 디버그 출력)
		char ipAddress[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(peerAddr.sin_addr), ipAddress, INET_ADDRSTRLEN);
		UINT port = ntohs(peerAddr.sin_port);

		// 디버그 출력 (선택 사항)
		CString debugMsg;
		debugMsg.Format(_T("IsConnected: IP = %hs, Port = %u\n"), ipAddress, port);
		OutputDebugString(debugMsg);

		return true;
	}
	else
	{
		// GetPeerName 실패 시 오류 코드 확인
		int err = GetLastError();
		if (err == WSAENOTCONN)
			return false;
		// 다른 오류는 연결 상태를 불확실하게 판단
		return false;
	}
}

// 클라이언트 소켓 보호 (크리티컬 섹션)
int CAISmartHomeDlg::GenerateUniqueClientID()
{
	static int currentID = 1;
	return currentID++;
}

void CAISmartHomeDlg::LockClientSockets()
{
	EnterCriticalSection(&m_ClientSocketsLock);
}

void CAISmartHomeDlg::UnlockClientSockets()
{
	LeaveCriticalSection(&m_ClientSocketsLock);
}

// 클라이언트 연결 수락 OnAccept 함수
void CMySocket::OnAccept(int nErrorCode)
{
	CAISmartHomeDlg* pDlg = (CAISmartHomeDlg*)AfxGetMainWnd();
	CMySocket* pClientSocket = new CMySocket();

	if (pDlg->m_serverSocket.Accept(*pClientSocket))
	{
		// 클라이언트 소켓에 고유 ID 할당
		pClientSocket->clientID = pDlg->GenerateUniqueClientID();

		pDlg->LockClientSockets();
		pDlg->m_ClientSockets.Add(pClientSocket);
		pDlg->UnlockClientSockets();

		CString logMessage;
		logMessage.Format(_T("OnAccept: Client %d connected. Total clients: %d\n"), pClientSocket->clientID, pDlg->m_ClientSockets.GetSize());
		OutputDebugString(logMessage);

		pClientSocket->m_bReadyToReceive = TRUE;
	}
	else
	{
		AfxMessageBox(_T("Failed to accept client connection"));
		delete pClientSocket;
	}

	CAsyncSocket::OnAccept(nErrorCode);
}

// OnReceive 함수 수정
void CMySocket::OnReceive(int nErrorCode)
{
	// 메인 윈도우 객체를 CLogWeather1Dlg 타입으로 캐스팅하여 가져옴
	CAISmartHomeDlg* pDlg = (CAISmartHomeDlg*)AfxGetMainWnd();

	// 소켓이 데이터를 받을 준비가 되어 있지 않다면 반환
	if (!m_bReadyToReceive)
	{
		CAsyncSocket::OnReceive(nErrorCode);
		return;
	}

	// 데이터를 수신하기 위한 버퍼 선언
	char buffer[1024] = { 0 };
	int bytesReceived = Receive(buffer, sizeof(buffer) - 1);

	// 수신 오류 처리
	if (bytesReceived == SOCKET_ERROR)
	{
		int err = GetLastError();
		// 오류 처리 로직 추가 가능
		CString errorMsg;
		errorMsg.Format(_T("Receive failed with error: %d\n"), err);
		OutputDebugString(errorMsg);
		CAsyncSocket::OnReceive(nErrorCode);
		return;
	}

	// 수신된 데이터가 있는 경우 처리
	if (bytesReceived > 0)
	{
		// 버퍼의 마지막에 NULL 문자 추가하여 CString으로 변환 가능하게 함
		buffer[bytesReceived] = '\0';
		CString receivedMessage(buffer);

		// 수신된 메시지를 로그로 출력
		CString logMessage;
		logMessage.Format(_T("Received message: %s\n"), receivedMessage);
		OutputDebugString(logMessage);

		// 수신된 메시지의 앞뒤 공백 제거
		receivedMessage.Trim();

		// 메시지 유형에 따라 핸들러 호출
		if (receivedMessage.Left(13).CompareNoCase(_T("VALIDATE_USER")) == 0)
		{
			// 예: "VALIDATE_USER,username"
			int commaPos = receivedMessage.Find(',');
			if (commaPos != -1 && commaPos + 1 < receivedMessage.GetLength())
			{
				CString username = receivedMessage.Mid(commaPos + 1).Trim(); // 공백 제거
				pDlg->ValidateUserHandle(this, username);
			}
			else
			{
				// 형식이 올바르지 않은 경우 오류 메시지 전송
				std::string responseMsg = "VALIDATE_FAILURE: Invalid Validation Data";
				std::vector<char> responseUTF8(responseMsg.begin(), responseMsg.end());
				responseUTF8.push_back('\n');

				pDlg->SendToClient(this, responseUTF8, _T("Validate Response"), clientID);

				// 로그
				CStringA sendLogA("Sent '");
				sendLogA += responseMsg.c_str();
				sendLogA += "' to client ";
				sendLogA += std::to_string(clientID).c_str();
				sendLogA += ".\n";
				CString sendLog(sendLogA);
				OutputDebugString(sendLog);
			}
		}
		else if (receivedMessage.Left(8).CompareNoCase(_T("REGISTER")) == 0)
		{
			// 예: "REGISTER,id,password,name,phone"
			pDlg->RegisterHandle(this, receivedMessage);
		}
		else if (receivedMessage.CompareNoCase(_T("Login_Data")) == 0)
		{
			// 로그인 데이터를 기다리는 플래그를 설정하고 로그 출력
			pDlg->SetWaitingForLoginData(true);
			OutputDebugString(_T("Waiting for login credentials...\n"));
		}
		else if (pDlg->IsWaitingForLoginData())
		{
			// 로그인 데이터를 처리하고 플래그를 해제
			pDlg->LoginHandle(this, receivedMessage);
			pDlg->SetWaitingForLoginData(false);  // 플래그 해제
		}
		else if (receivedMessage.CompareNoCase(_T("REQUEST_DATA")) == 0)
		{
			// 날씨 데이터 요청 처리
			pDlg->WeatherHandle(this);
		}
		else if (receivedMessage.CompareNoCase(_T("REQUEST_SENSORDATA")) == 0)
		{
			// 센서 데이터 요청 처리, 현재 소켓에 센서 데이터 전송
			pDlg->SensorDataHandle(this);
		}
		else if (receivedMessage.CompareNoCase(_T("MAIN_DATA")) == 0)
		{
			// 날씨 데이터 요청 처리
			pDlg->MainHandle(this);
		}
		else if (receivedMessage.CompareNoCase(_T("PFAN_ON")) == 0)
		{
			// 공기청정기 팬 켜기
			pDlg->SendPFANON();
		}
		else if (receivedMessage.CompareNoCase(_T("PFAN_OFF")) == 0)
		{
			// 공기청정기 팬 끄기
			pDlg->SendPFANOFF();
		}
		else if (receivedMessage.CompareNoCase(_T("HFAN_ON")) == 0)
		{
			// 난방 팬 켜기
			pDlg->SendHFANON();
		}
		else if (receivedMessage.CompareNoCase(_T("HFAN_OFF")) == 0)
		{
			// 난방 팬 끄기
			pDlg->SendHFANOFF();
		}
		else if (receivedMessage.CompareNoCase(_T("AC_ON")) == 0)
		{
			// 에어컨 팬 켜기
			pDlg->SendACON();
		}
		else if (receivedMessage.CompareNoCase(_T("AC_OFF")) == 0)
		{
			// 에어컨 팬 끄기
			pDlg->SendACOFF();
		}
		else if (receivedMessage.CompareNoCase(_T("LED_ON")) == 0)
		{
			// 조명 켜기
			pDlg->SendLEDON();
		}
		else if (receivedMessage.CompareNoCase(_T("LED_OFF")) == 0)
		{
			// 조명 끄기
			pDlg->SendLEDOFF();
		}
		else if (receivedMessage.CompareNoCase(_T("Recommend_ON")) == 0)
		{
			// 추천온도 데이터 요청 처리, 현재 외부온도와 내부온도를 통해 추천온도 안내
			pDlg->RecommendTEMPON();
		}
		else if (receivedMessage.CompareNoCase(_T("Recommend_OFF")) == 0)
		{
			// 추천온도 데이터 요청 처리, 현재 외부온도와 내부온도를 통해 추천온도 안내
			pDlg->RecommendTEMPOFF();
		}
		else if (receivedMessage.CompareNoCase(_T("UserTEMP_OFF")) == 0)
		{
			// 추천온도 데이터 요청 처리, 현재 외부온도와 내부온도를 통해 추천온도 안내
			pDlg->SendUSERTEMPOFF();
		}
		else if (receivedMessage.CompareNoCase(_T("Tip_Data")) == 0)
		{
			// 웹 API에서 날씨 데이터, 미세먼지 데이터, 자외선 데이터를 가져옵니다
			CString weatherJson = pDlg->GetWeatherData();
			CString microJson = pDlg->GetMicroData();
			CString uvJson = pDlg->GetUVData();

			// JSON 데이터를 파싱하여 구조체에 저장합니다
			WeatherInfo info = pDlg->ParseWeatherData(weatherJson);
			MicroInfo Minfo = pDlg->ParseMicroData(microJson);
			UVInfo UVinfo = pDlg->ParseUVData(uvJson);

			// SendWeatherDataToPython 함수 호출
			pDlg->SendWeatherDataToPython(this, info, Minfo, UVinfo);
		}
		else if (receivedMessage.Left(11).CompareNoCase(_T("UsersetTEMP")) == 0)
		{
			// 예: "USERTEMP,usersetTEMP" 사용자 설정 온도
			int commaPos = receivedMessage.Find(',');
			if (commaPos != -1 && commaPos + 1 < receivedMessage.GetLength())
			{
				CString usersetTEMP = receivedMessage.Mid(commaPos + 1).Trim(); // 공백 제거
				pDlg->UsersetTEMPHandle(this, usersetTEMP);
			}
			else
			{
				// 형식이 올바르지 않은 경우 오류 메시지 전송
				std::string responseMsg = "UsersetTEMP_FAILURE: Invalid UsersetTEMP Data";
				std::vector<char> responseUTF8(responseMsg.begin(), responseMsg.end());
				responseUTF8.push_back('\n');

				pDlg->SendToClient(this, responseUTF8, _T("UsersetTEMP Response"), clientID);

				// 로그
				CStringA sendLogA("Sent '");
				sendLogA += responseMsg.c_str();
				sendLogA += "' to client ";
				sendLogA += std::to_string(clientID).c_str();
				sendLogA += ".\n";
				CString sendLog(sendLogA);
				OutputDebugString(sendLog);
			}
		}
		else
		{
			// 기타 등록 관련 메시지 처리
			pDlg->RegisterHandle(this, receivedMessage);
		}
	}

	// 기본 클래스의 OnReceive 함수 호출
	CAsyncSocket::OnReceive(nErrorCode);
}

// 클라이언트 소켓 닫는 OnClose 함수
void CMySocket::OnClose(int nErrorCode)
{
	CAISmartHomeDlg* pDlg = (CAISmartHomeDlg*)AfxGetMainWnd();

	pDlg->LockClientSockets();
	for (int i = 0; i < pDlg->m_ClientSockets.GetSize(); ++i)
	{
		if (pDlg->m_ClientSockets[i] == this)
		{
			pDlg->m_ClientSockets.RemoveAt(i);
			break;
		}
	}
	pDlg->UnlockClientSockets();

	CString logMessage;
	logMessage.Format(_T("OnClose: Client %d disconnected. Remaining clients: %d\n"), clientID, pDlg->m_ClientSockets.GetSize());
	OutputDebugString(logMessage);

	CAsyncSocket::OnClose(nErrorCode);
}

// SendToClient 함수 구현
void CAISmartHomeDlg::SendToClient(CMySocket* pClientSocket, const std::vector<char>& utf8Data, const CString& label, int clientID)
{
	if (pClientSocket->m_hSocket == INVALID_SOCKET)
	{
		CString errorMsg;
		errorMsg.Format(_T("Invalid socket detected, skipping client %d.\n"), clientID);
		OutputDebugString(errorMsg);
		return;
	}

	// 데이터를 클라이언트에 전송
	int bytesSent = pClientSocket->Send(utf8Data.data(), static_cast<int>(utf8Data.size()));

	// 전송된 바이트 수와 클라이언트 정보를 로그로 출력
	CString logMessage;
	logMessage.Format(_T("Sent %d bytes to client %d: %s\n"), bytesSent, clientID, label);
	//OutputDebugString(logMessage);

	// 전송된 데이터 내용을 로그로 출력 (정확한 크기로 문자열 생성)
	std::string dataSent(utf8Data.data(), utf8Data.size());
	CStringA dataLogA("Data sent: ");
	dataLogA += dataSent.c_str();
	dataLogA += "\n";
	CString dataLog(dataLogA);
	//OutputDebugString(dataLog);

	// 전송 오류 처리
	if (bytesSent == SOCKET_ERROR)
	{
		int err = GetLastError();
		CString errorMsg;
		errorMsg.Format(_T("Failed to send data '%s' to client %d. Error: %d\n"), label, clientID, err);
		OutputDebugString(errorMsg);
	}
}

// 날씨 데이터 전송 WeatherHandle 함수
void CAISmartHomeDlg::WeatherHandle(CMySocket* pClientSocket)
{
	OutputDebugString(_T("WeatherHandle: Function Start\n"));

	try {
		// 개별 날씨 데이터 포맷팅
		CString skyData, tempData, ptyData, popData, rehData, uvData, microData;

		skyData.Format(_T("SKY: %s\n"), this->SKY_D);
		tempData.Format(_T("TEMP: %s\n"), this->TEMP_D);
		ptyData.Format(_T("PTY: %s\n"), this->PTY_D);
		popData.Format(_T("POP: %s\n"), this->POP_D);
		rehData.Format(_T("REH: %s\n"), this->REH_D);
		uvData.Format(_T("UV: %s\n"), this->UV_D);
		microData.Format(_T("MICRO: %s\n"), this->MICRO_D);

		// 각 데이터를 UTF-8 바이트 배열로 변환하는 람다 함수 정의
		auto convertToUTF8 = [](const CString& data) -> std::vector<char> {
			CStringW unicodeData(data);
			// UTF-8로 변환할 때 필요한 버퍼 크기 계산
			int utf8Length = WideCharToMultiByte(CP_UTF8, 0, unicodeData.GetString(), -1, NULL, 0, NULL, NULL);
			std::vector<char> utf8Data(utf8Length);
			if (utf8Length > 0) {
				// 실제 UTF-8 변환 수행
				WideCharToMultiByte(CP_UTF8, 0, unicodeData.GetString(), -1, utf8Data.data(), utf8Length, NULL, NULL);
			}
			return utf8Data;
		};

		// 각 데이터를 UTF-8로 변환
		std::vector<char> skyUTF8 = convertToUTF8(skyData);
		std::vector<char> tempUTF8 = convertToUTF8(tempData);
		std::vector<char> ptyUTF8 = convertToUTF8(ptyData);
		std::vector<char> popUTF8 = convertToUTF8(popData);
		std::vector<char> rehUTF8 = convertToUTF8(rehData);
		std::vector<char> uvUTF8 = convertToUTF8(uvData);
		std::vector<char> microUTF8 = convertToUTF8(microData);

		// 특정 클라이언트 소켓에 변환된 데이터 전송
		if (pClientSocket != nullptr && pClientSocket->IsConnected())
		{
			SendToClient(pClientSocket, skyUTF8, _T("SKY"), pClientSocket->clientID);
			SendToClient(pClientSocket, tempUTF8, _T("TEMP"), pClientSocket->clientID);
			SendToClient(pClientSocket, ptyUTF8, _T("PTY"), pClientSocket->clientID);
			SendToClient(pClientSocket, popUTF8, _T("POP"), pClientSocket->clientID);
			SendToClient(pClientSocket, rehUTF8, _T("REH"), pClientSocket->clientID);
			SendToClient(pClientSocket, uvUTF8, _T("UV"), pClientSocket->clientID);
			SendToClient(pClientSocket, microUTF8, _T("MICRO"), pClientSocket->clientID);
		}
		else
		{
			CString errorMsg;
			errorMsg.Format(_T("WeatherHandle: 클라이언트 %d가 연결되어 있지 않습니다.\n"), pClientSocket->clientID);
			OutputDebugString(errorMsg);
		}
	}
	catch (CException* e) {
		// 예외 발생 시 처리 함수 호출
		HandleException(e);
	}
	OutputDebugString(_T("WeatherHandle: Function End\n"));
}

// 메인 화면 필요 데이터 전송 MainHandle 함수
void CAISmartHomeDlg::MainHandle(CMySocket* pClientSocket)
{
	OutputDebugString(_T("MainHandle: Function Start\n"));

	try {
		// 센서 데이터 포맷팅
		CString outtempData, intempData, humData, rehData, microData;

		outtempData.Format(_T("OUTTEMP: %s\n"), this->TEMP_D);
		rehData.Format(_T("REH: %s\n"), this->REH_D);
		microData.Format(_T("MICRO: %s\n"), this->MICRO_D);
		intempData.Format(_T("INTEMP: %.2f\n"), this->g_temp);
		humData.Format(_T("HUM: %.2f\n"), this->g_hum);

		// UTF-8로 변환하는 람다 함수 정의
		auto convertToUTF8 = [](const CString& str) -> std::vector<char> {
			CStringW unicodeData(str);
			// UTF-8로 변환할 때 필요한 버퍼 크기 계산
			int utf8Length = WideCharToMultiByte(CP_UTF8, 0, unicodeData.GetString(), -1, NULL, 0, NULL, NULL);
			std::vector<char> utf8Data(utf8Length);
			if (utf8Length > 0) {
				// 실제 UTF-8 변환 수행
				WideCharToMultiByte(CP_UTF8, 0, unicodeData.GetString(), -1, utf8Data.data(), utf8Length, NULL, NULL);
			}
			return utf8Data;
		};

		// 각 센서 데이터를 UTF-8로 변환
		std::vector<char> OUTtempUTF8 = convertToUTF8(outtempData);
		std::vector<char> INtempUTF8 = convertToUTF8(intempData);
		std::vector<char> microUTF8 = convertToUTF8(microData);
		std::vector<char> humUTF8 = convertToUTF8(humData);
		std::vector<char> rehUTF8 = convertToUTF8(rehData);

		// 해당 클라이언트 소켓에 변환된 센서 데이터 전송
		SendToClient(pClientSocket, OUTtempUTF8, _T("OUTTEMP"), pClientSocket->clientID);
		SendToClient(pClientSocket, INtempUTF8, _T("INTEMP"), pClientSocket->clientID);
		SendToClient(pClientSocket, microUTF8, _T("MICRO"), pClientSocket->clientID);
		SendToClient(pClientSocket, humUTF8, _T("HUM"), pClientSocket->clientID);
		SendToClient(pClientSocket, rehUTF8, _T("REH"), pClientSocket->clientID);
	}
	catch (CException* e) {
		// 예외 발생 시 처리 함수 호출
		HandleException(e);
	}

	OutputDebugString(_T("MainHandle: Function End\n"));
}

// 센서 데이터 전송 SensorDataHandle 함수
void CAISmartHomeDlg::SensorDataHandle(CMySocket* pClientSocket)
{
	OutputDebugString(_T("SensorDataHandle: Function Start\n"));

	try {
		// 센서 데이터 포맷팅
		CString tempData, humData, dustData;

		tempData.Format(_T("TEMP: %.2f\n"), this->g_temp);
		humData.Format(_T("HUM: %.2f\n"), this->g_hum);
		dustData.Format(_T("DUST: %.2f\n"), this->g_dust);


		// UTF-8로 변환하는 람다 함수 정의
		auto convertToUTF8 = [](const CString& str) -> std::vector<char> {
			CStringW unicodeData(str);
			// UTF-8로 변환할 때 필요한 버퍼 크기 계산
			int utf8Length = WideCharToMultiByte(CP_UTF8, 0, unicodeData.GetString(), -1, NULL, 0, NULL, NULL);
			std::vector<char> utf8Data(utf8Length);
			if (utf8Length > 0) {
				// 실제 UTF-8 변환 수행
				WideCharToMultiByte(CP_UTF8, 0, unicodeData.GetString(), -1, utf8Data.data(), utf8Length, NULL, NULL);
			}
			return utf8Data;
		};

		// 각 센서 데이터를 UTF-8로 변환
		std::vector<char> tempUTF8 = convertToUTF8(tempData);
		std::vector<char> humUTF8 = convertToUTF8(humData);
		std::vector<char> dustUTF8 = convertToUTF8(dustData);


		// 해당 클라이언트 소켓에 변환된 센서 데이터 전송
		SendToClient(pClientSocket, tempUTF8, _T("TEMP"), pClientSocket->clientID);
		SendToClient(pClientSocket, humUTF8, _T("HUM"), pClientSocket->clientID);
		SendToClient(pClientSocket, dustUTF8, _T("DUST"), pClientSocket->clientID);
	}
	catch (CException* e) {
		// 예외 발생 시 처리 함수 호출
		HandleException(e);
	}

	OutputDebugString(_T("SensorDataHandle: Function End\n"));
}

// 로그인 데이터 전송 LoginHandle 함수
void CAISmartHomeDlg::LoginHandle(CMySocket* pSenderSocket, const CString& loginMessage)
{
	OutputDebugString(_T("LoginHandle: Function Start\n"));

	// 로그인 메시지에서 ',' 위치 찾기
	int commaPos = loginMessage.Find(',');
	if (commaPos != -1) {
		// ','를 기준으로 ID와 패스워드 분리
		CString id = loginMessage.Left(commaPos);
		CString password = loginMessage.Mid(commaPos + 1);

		// 분리된 ID와 패스워드를 로그로 출력
		CString logMsg;
		logMsg.Format(_T("Extracted ID: %s, Password: %s\n"), id, password);
		OutputDebugString(logMsg);

		// SQL 쿼리 작성 (사용자 인증)
		CString query;
		query.Format(_T("SELECT * FROM smartlogin WHERE id = '%s' AND password = '%s'"), id, password);

		// 작성된 쿼리를 로그로 출력
		CString queryLog;
		queryLog.Format(_T("Executing query: %s\n"), (LPCTSTR)query);
		OutputDebugString(queryLog);

		// MySQL 쿼리 실행
		query_state = mysql_query(Connection, (LPCTSTR)query);

		std::string responseMsg;
		if (!query_state) {
			// 쿼리 결과 저장
			SQL_result = mysql_store_result(Connection);
			int num_rows = mysql_num_rows(SQL_result);

			// 반환된 행 수를 로그로 출력
			CString logMsg;
			logMsg.Format(_T("Number of rows returned: %d\n"), num_rows);
			OutputDebugString(logMsg);

			// 로그인 성공 여부 결정
			responseMsg = (num_rows > 0) ? "LOGIN_SUCCESS" : "LOGIN_FAILURE";

			// 로그인 상태를 로그로 출력
			if (num_rows > 0) {
				OutputDebugString(_T("Login status: Login Successful\n"));
			}
			else {
				OutputDebugString(_T("Login status: Login Failed\n"));
			}

			// 결과 메모리 해제
			mysql_free_result(SQL_result);
		}
		else {
			// 쿼리 실행 오류 발생 시
			responseMsg = "LOGIN_FAILURE: Query Error";
		}

		// 응답 메시지를 로그로 출력
		CString responseLog;
		responseLog.Format(_T("Response message: %s\n"), CString(responseMsg.c_str()));
		OutputDebugString(responseLog);

		// 응답 메시지를 UTF-8 바이트 배열로 변환
		std::vector<char> responseUTF8(responseMsg.begin(), responseMsg.end());
		responseUTF8.push_back('\n');  // 메시지 끝에 개행 문자 추가
		responseUTF8.push_back('\0');  // 널 종료 문자 추가

		// 요청을 보낸 클라이언트에게만 응답 전송
		if (pSenderSocket != NULL) {
			SendToClient(pSenderSocket, responseUTF8, _T("Login Response"), pSenderSocket->clientID);
		}
	}

	// 로그인 처리 후 대기 플래그를 false로 설정
	m_bWaitingForLoginData = false;
	// 플래그 변경을 로그로 출력
	OutputDebugString(_T("m_bWaitingForLoginData set to false after handling login data.\n"));

	OutputDebugString(_T("LoginHandle: Function End\n"));
}

// 예외 발생 처리 HandleException 함수
void CAISmartHomeDlg::HandleException(CException* e)
{
	// 예외 발생을 로그로 출력
	OutputDebugString(_T("CException caught\n"));
	// 예외 상세 정보를 보고
	e->ReportError();
	// 예외 객체 삭제
	e->Delete();
}

// UTF8을 CString으로 변환하는 UTF8ToCString 함수
CString CAISmartHomeDlg::UTF8ToCString(const std::string& utf8Str)
{
	// UTF-8 문자열을 WideChar (유니코드)로 변환할 때 필요한 wchar_t 개수 계산
	int wcharCount = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
	if (wcharCount == 0) {
		return _T("");
	}

	// wchar_t 버퍼에 변환된 문자열 저장
	std::vector<wchar_t> wcharStr(wcharCount);
	MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, wcharStr.data(), wcharCount);

	// wchar_t 배열을 CString으로 반환
	return CString(wcharStr.data());
}

// ValidateUserHandle 함수 구현
void CAISmartHomeDlg::ValidateUserHandle(CMySocket* pSenderSocket, const CString& username)
{
	OutputDebugString(_T("ValidateUserHandle: Function Start\n"));

	try
	{
		// 사용자 이름을 로그로 출력 (공백 제거)
		CString trimmedUsername = username; // 원본을 복사
		trimmedUsername.Trim();             // 복사본을 수정

		// 사용자 이름을 로그로 출력
		CString logMsg;
		logMsg.Format(_T("Validating username: %s\n"), trimmedUsername);
		OutputDebugString(logMsg);

		// SQL 쿼리 작성 (사용자 중복 확인)
		CString query;
		query.Format(_T("SELECT COUNT(*) FROM smartlogin WHERE id = '%s'"), trimmedUsername);

		// 쿼리 실행
		query_state = mysql_query(Connection, CT2CA(query));

		std::string responseMsg;
		if (!query_state)
		{
			// 쿼리 결과 저장
			MYSQL_RES* result = mysql_store_result(Connection);
			if (result)
			{
				MYSQL_ROW row = mysql_fetch_row(result);
				int count = atoi(row[0]);

				// 결과를 로그로 출력
				logMsg.Format(_T("Number of users with ID '%s': %d\n"), trimmedUsername, count);
				OutputDebugString(logMsg);

				// 중복 여부 결정
				if (count > 0)
				{
					responseMsg = "VALIDATE FAILURE"; // 실패
					OutputDebugString(_T("ValidateUserHandle: Username already exists.\n"));
				}
				else
				{
					responseMsg = "VALIDATE SUCCESS"; // 성공
					OutputDebugString(_T("ValidateUserHandle: Username is available.\n"));
				}

				// 결과 메모리 해제
				mysql_free_result(result);
			}
			else
			{
				// 결과 저장 실패
				responseMsg = "VALIDATE_FAILURE: Unable to retrieve query result";
				OutputDebugString(_T("ValidateUserHandle: Unable to retrieve query result.\n"));
			}
		}
		else
		{
			// 쿼리 실행 오류 발생 시
			responseMsg = "VALIDATE_FAILURE: Query Error";
			OutputDebugString(_T("ValidateUserHandle: SQL query failed.\n"));
		}

		// 응답 메시지를 로그로 출력 (인코딩 수정)
		CStringA responseLogA("Response message to send: ");
		responseLogA += responseMsg.c_str();
		responseLogA += "\n";
		CString responseLog(responseLogA);
		OutputDebugString(responseLog);

		// 응답 메시지를 UTF-8 바이트 배열로 변환
		std::vector<char> responseUTF8(responseMsg.begin(), responseMsg.end());
		responseUTF8.push_back('\n');  // 메시지 끝에 개행 문자 추가
		responseUTF8.push_back('\0');  // 널 종료 문자 추가

		// 요청을 보낸 클라이언트에게만 응답 전송
		if (pSenderSocket != NULL)
		{
			SendToClient(pSenderSocket, responseUTF8, _T("Validate Response"), pSenderSocket->clientID);

			// 전송 후 로그 추가: 어떤 메시지를 어떤 클라이언트에게 보냈는지 기록
			CStringA sendLogA("Sent '");
			sendLogA += responseMsg.c_str();
			sendLogA += "' to client ";
			sendLogA += std::to_string(pSenderSocket->clientID).c_str();
			sendLogA += ".\n";
			CString sendLog(sendLogA);
			OutputDebugString(sendLog);
		}
		else
		{
			OutputDebugString(_T("ValidateUserHandle: pSenderSocket is NULL. Cannot send response.\n"));
		}
	}
	catch (CException* e)
	{
		// 예외 발생 시 처리 함수 호출
		HandleException(e);
	}

	OutputDebugString(_T("ValidateUserHandle: Function End\n"));
}

// RegisterHandle 함수 
void CAISmartHomeDlg::RegisterHandle(CMySocket* pSenderSocket, const CString& registerMessage)
{
	OutputDebugString(_T("RegisterHandle: Function Start\n"));

	try
	{
		// 데이터 토큰화를 위한 변수 초기화
		int curPos = 0;
		CString token;
		CString data(registerMessage);

		// 토큰화된 데이터를 저장할 배열 (REGISTER, id, password, name, phone)
		std::string tokens[5];
		int tokenIndex = 0;

		// ','를 기준으로 데이터를 분리하여 tokens 배열에 저장
		token = data.Tokenize(_T(","), curPos);
		while (!token.IsEmpty() && tokenIndex < 5) {
			tokens[tokenIndex++] = std::string(CT2CA(token));
			token = data.Tokenize(_T(","), curPos);
		}

		// 모든 데이터가 잘 분리되었는지 확인 (5개의 데이터 필요: REGISTER, id, password, name, phone)
		std::string responseMsg;
		if (tokenIndex == 5 && tokens[0] == "REGISTER")
		{
			// tokens 배열의 각 요소를 사용하여 SQL INSERT 쿼리 작성
			// tokens[1] = id, tokens[4] = password, tokens[2] = name, tokens[3] = phone
			CString query;
			query.Format(_T("INSERT INTO smartlogin(id, password, name, phone) VALUES('%s', '%s', '%s', '%s')"),
				tokens[1].c_str(), tokens[4].c_str(), tokens[2].c_str(), tokens[3].c_str());

			// 쿼리 실행
			query_state = mysql_query(Connection, CT2CA(query));

			if (query_state != 0)
			{
				// 쿼리 실행 오류 발생 시 에러 메시지 설정
				responseMsg = "REGISTER FAILURE";
				OutputDebugString(_T("RegisterHandle: SQL query failed.\n"));
			}
			else
			{
				// 쿼리 실행 성공 시 성공 메시지 설정
				responseMsg = "REGISTER SUCCESS";
				OutputDebugString(_T("RegisterHandle: SQL query succeeded.\n"));
			}
		}
		else
		{
			// 토큰화 실패 시 에러 메시지 설정
			responseMsg = "REGISTER_FAILURE: Invalid Registration Data";
			OutputDebugString(_T("RegisterHandle: Invalid registration data format.\n"));
		}

		// 응답 메시지를 로그로 출력 (인코딩 수정)
		CStringA responseLogA("Response message to send: ");
		responseLogA += responseMsg.c_str();
		responseLogA += "\n";
		CString responseLog(responseLogA);
		OutputDebugString(responseLog);

		// 응답 메시지를 UTF-8 바이트 배열로 변환
		std::vector<char> responseUTF8(responseMsg.begin(), responseMsg.end());
		responseUTF8.push_back('\n');  // 메시지 끝에 개행 문자 추가
		responseUTF8.push_back('\0');  // 널 종료 문자 추가

		// 요청을 보낸 클라이언트에게만 응답 전송
		if (pSenderSocket != NULL)
		{
			SendToClient(pSenderSocket, responseUTF8, _T("Register Response"), pSenderSocket->clientID);

			// 전송 후 로그 추가: 어떤 메시지를 어떤 클라이언트에게 보냈는지 기록 (인코딩 수정)
			CStringA sendLogA("Sent '");
			sendLogA += responseMsg.c_str();
			sendLogA += "' to client ";
			sendLogA += std::to_string(pSenderSocket->clientID).c_str();
			sendLogA += ".\n";
			CString sendLog(sendLogA);
			OutputDebugString(sendLog);
		}
		else
		{
			OutputDebugString(_T("RegisterHandle: pSenderSocket is NULL. Cannot send response.\n"));
		}
	}
	catch (CException* e)
	{
		// 예외 발생 시 처리 함수 호출
		HandleException(e);
	}

	OutputDebugString(_T("RegisterHandle: Function End\n"));
}

// 로그인 대기 상태 설정 SetWaitingForLoginData 함수
void CAISmartHomeDlg::SetWaitingForLoginData(bool isWaiting)
{
	// 로그인 데이터를 기다리는 상태를 설정
	m_bWaitingForLoginData = isWaiting;
}

// 로그인 대기 상태 확인 IsWaitingForLoginData 함수
bool CAISmartHomeDlg::IsWaitingForLoginData() const
{
	// 로그인 데이터를 기다리는 상태를 반환
	return m_bWaitingForLoginData;
}

// 사용할 팬 목록 : 공기청정기, 난방, 에어컨
// 공기청정기(미세먼지) on/off (아두이노 명령 전송)
void CAISmartHomeDlg::SendPFANON()
{
	// ANSI 문자열로 변환
	CStringA PfanCommandA("PFan ON\n"); // '\n' 추가
	const char* sendBuffer = PfanCommandA.GetString();

	//AfxMessageBox(_T("데이터 송신을 시도합니다."));

	BOOL bSuccess = m_serial.WriteData(sendBuffer, PfanCommandA.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
	}
	else {
		AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
	}

}

void CAISmartHomeDlg::SendPFANOFF()
{
	// ANSI 문자열로 변환
	CStringA PfanCommandA("PFan OFF\n"); // '\n' 추가
	const char* sendBuffer = PfanCommandA.GetString();

	//AfxMessageBox(_T("데이터 송신을 시도합니다."));

	BOOL bSuccess = m_serial.WriteData(sendBuffer, PfanCommandA.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
	}
	else {
		AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
	}

}

// 에어컨 on/off (아두이노 명령 전송)
void CAISmartHomeDlg::SendACON()
{
	// ANSI 문자열로 변환
	CStringA AcCommandA("AC ON\n"); // '\n' 추가
	const char* sendBuffer = AcCommandA.GetString();

	//AfxMessageBox(_T("데이터 송신을 시도합니다."));

	BOOL bSuccess = m_serial.WriteData(sendBuffer, AcCommandA.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
	}
	else {
		AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
	}

}

void CAISmartHomeDlg::SendACOFF()
{
	// ANSI 문자열로 변환
	CStringA AcCommandA("AC OFF\n"); // '\n' 추가
	const char* sendBuffer = AcCommandA.GetString();

	//AfxMessageBox(_T("데이터 송신을 시도합니다."));

	BOOL bSuccess = m_serial.WriteData(sendBuffer, AcCommandA.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
	}
	else {
		AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
	}

}

// 난방 on/off (아두이노 명령 전송)
void CAISmartHomeDlg::SendHFANON()
{
	// ANSI 문자열로 변환
	CStringA HfanCommandA("HFan ON\n"); // '\n' 추가
	const char* sendBuffer = HfanCommandA.GetString();

	//AfxMessageBox(_T("데이터 송신을 시도합니다."));

	BOOL bSuccess = m_serial.WriteData(sendBuffer, HfanCommandA.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
	}
	else {
		AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
	}

}

void CAISmartHomeDlg::SendHFANOFF()
{
	// ANSI 문자열로 변환
	CStringA HfanCommandA("HFan OFF\n"); // '\n' 추가
	const char* sendBuffer = HfanCommandA.GetString();

	//AfxMessageBox(_T("데이터 송신을 시도합니다."));

	BOOL bSuccess = m_serial.WriteData(sendBuffer, HfanCommandA.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
	}
	else {
		AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
	}

}

// LED(조명) on/off(아두이노 명령 전송)
void CAISmartHomeDlg::SendLEDON()
{
	// ANSI 문자열로 변환
	CStringA LEDCommandA("LED ON\n"); // '\n' 추가
	const char* sendBuffer = LEDCommandA.GetString();

	//AfxMessageBox(_T("데이터 송신을 시도합니다."));

	BOOL bSuccess = m_serial.WriteData(sendBuffer, LEDCommandA.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
	}
	else {
		AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
	}

}

void CAISmartHomeDlg::SendLEDOFF()
{
	// ANSI 문자열로 변환
	CStringA LEDCommandA("LED OFF\n"); // '\n' 추가
	const char* sendBuffer = LEDCommandA.GetString();

	//AfxMessageBox(_T("데이터 송신을 시도합니다."));

	BOOL bSuccess = m_serial.WriteData(sendBuffer, LEDCommandA.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
	}
	else {
		AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
	}

}

void CAISmartHomeDlg::SendUSERTEMPOFF()
{
	// ANSI 문자열로 변환
	CStringA LEDCommandA("USERTEMP OFF\n"); // '\n' 추가
	const char* sendBuffer = LEDCommandA.GetString();

	//AfxMessageBox(_T("데이터 송신을 시도합니다."));

	BOOL bSuccess = m_serial.WriteData(sendBuffer, LEDCommandA.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
	}
	else {
		AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
	}

}

void CAISmartHomeDlg::UsersetTEMPHandle(CMySocket* pClientSocket, const CString& usersetTEMP)
{

	// 1. CString을 정수로 변환
	int tempValue = _ttoi(usersetTEMP);

	// 2. 데이터 검증 (예: 온도 범위 0~100)
	if (tempValue < 0 || tempValue > 100)
	{
		// 로그 기록
		CStringA logMsgA;
		logMsgA.Format("Invalid temperature value received: %d from client %d\n", tempValue, pClientSocket->clientID);
		OutputDebugStringA(logMsgA);

		return;
	}

	// 3. Arduino로 전송할 데이터 준비 (예: "UsersetTEMP,%d\n")
	CStringA UsersetTEMPData;
	UsersetTEMPData.Format("UsersetTEMP,%d\n", tempValue); // Arduino가 정수 값을 수신하도록 포맷

// 사용자 설정 함수 아두이노 전송
	const char* sendBuffer = UsersetTEMPData.GetString();

	CString RR;
	RR.Format(_T("데이터 송신을 시도합니다. %s"), UsersetTEMPData.GetString());
	OutputDebugStringA(RR);

	BOOL bSuccess = m_serial.WriteData(sendBuffer, UsersetTEMPData.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
		OutputDebugString(_T("Data Ok\n"));
	}
	else {
		//AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
		OutputDebugString(_T("Data Fail\n"));
	}
}

// 버튼 on/off시 클라이언트로 데이터 전송 함수
void CAISmartHomeDlg::SendButtonState(bool state)
{
	// 전송할 메시지 설정
	CString message = state ? _T("BTN_ON\n") : _T("BTN_OFF\n");

	// CString을 UTF-8 인코딩된 std::vector<char>로 변환
	std::vector<char> utf8Data = CStringToUTF8Vector(message);

	// 클라이언트 소켓 리스트에 접근하기 전에 락을 획득
	LockClientSockets();

	// 모든 클라이언트 소켓에 메시지 전송
	for (int i = 0; i < m_ClientSockets.GetSize(); ++i)
	{
		CMySocket* pClientSocket = m_ClientSockets[i];
		if (pClientSocket->IsConnected())
		{
			SendToClient(pClientSocket, utf8Data, message, pClientSocket->clientID);
		}
	}

	// 클라이언트 소켓 리스트 접근 후 락 해제
	UnlockClientSockets();
}

// 파이썬 서버 사용 함수
bool CAISmartHomeDlg::ConnectToServer()
{
	try
	{
		if (!pServer)
		{
			pServer = m_session.GetHttpConnection(_T("localhost"), NULL, 8000);

			// 파이썬 서버에 연결 요청
			CHttpFile* pFile = pServer->OpenRequest(CHttpConnection::HTTP_VERB_POST, _T("/connect"));
			pFile->SendRequest();
			pFile->Close();
			delete pFile;
		}
		return true;
	}
	catch (CInternetException* e)
	{
		TCHAR szErr[1024];
		e->GetErrorMessage(szErr, 1024);
		AfxMessageBox(szErr);
		e->Delete();
		return false;
	}
}

void CAISmartHomeDlg::DisconnectFromServer()
{
	if (pServer)
	{
		try
		{
			// 파이썬 서버에 연결 해제 요청
			CHttpFile* pFile = pServer->OpenRequest(CHttpConnection::HTTP_VERB_POST, _T("/disconnect"));
			pFile->SendRequest();
			pFile->Close();
			delete pFile;
		}
		catch (CInternetException* e)
		{
			TCHAR szErr[1024];
			e->GetErrorMessage(szErr, 1024);
			AfxMessageBox(szErr);
			e->Delete();
		}

		pServer->Close();
		delete pServer;
		pServer = nullptr;
	}
}

CString UTF8toCString(const std::string& utf8Str)
{
	int wcharCount = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
	if (wcharCount == 0) {
		return _T(""); // 변환 실패
	}

	std::vector<wchar_t> wcharStr(wcharCount);
	MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, wcharStr.data(), wcharCount);

	return CString(wcharStr.data());
}

// CString 형식의 문자열을 UTF8로 변환, 파이썬에서 문자열 전송할때 필요
std::string ConvertCStringToUTF8(const CString& str) {
	// CStringW를 사용하여 유니코드 문자열로 변환
	CStringW unicodeStr(str);

	// 유니코드 문자열을 UTF-8로 변환
	int utf8Length = WideCharToMultiByte(CP_UTF8, 0, unicodeStr.GetString(), -1, NULL, 0, NULL, NULL);
	if (utf8Length > 0) {
		std::vector<char> utf8Str(utf8Length);
		WideCharToMultiByte(CP_UTF8, 0, unicodeStr.GetString(), -1, utf8Str.data(), utf8Length, NULL, NULL);
		return std::string(utf8Str.data());
	}

	return "";
}

#define OUTPUT_ERROR -4.0	//서버와 통신 실패시 대신 출력될 결과값

// 딥러닝 학습 실행 함수 
UINT TrainModelThread(LPVOID pParam)
{
	CAISmartHomeDlg* pDlg = (CAISmartHomeDlg*)pParam;
	if (!pDlg->ConnectToServer())
	{
		return 1;
	}

	// 서버에 학습 요청 전송
	pDlg->SendTrainModelRequest();

	return 0;
}
void CAISmartHomeDlg::SendTrainModelRequest()
{
	CHttpFile* pFile = nullptr;

	try
	{
		pFile = pServer->OpenRequest(CHttpConnection::HTTP_VERB_POST, _T("/train_model"));

		// 요청 전송
		pFile->SendRequest();

		// 상태 코드 확인
		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		if (dwStatusCode == HTTP_STATUS_OK)
		{
			std::string responseStr;
			char buffer[4096] = { 0 };

			// 서버 응답을 읽어옴
			while (pFile->Read(buffer, sizeof(buffer) - 1))
			{
				buffer[sizeof(buffer) - 1] = '\0';
				responseStr += buffer;
			}

			// JSON 파싱
			rapidjson::Document document;
			if (document.Parse(responseStr.c_str()).HasParseError())
			{
				AfxMessageBox(_T("JSON 파싱 오류가 발생했습니다."));
			}
			else if (document.HasMember("message"))
			{
				CString studyMessage = CString(document["message"].GetString());

				// 결과 메시지 출력
				CString message;
				message.Format(_T("결과: %s"), studyMessage);
				MessageBox(message, _T("Micro Dust Information"), MB_OK | MB_ICONINFORMATION);

			}
			else
			{
				AfxMessageBox(_T("응답 형식이 올바르지 않습니다."));
			}
		}
		else
		{
			AfxMessageBox(_T("서버로부터 실패 상태 코드를 받았습니다."));
		}
	}
	catch (CInternetException* e)
	{
		TCHAR szErr[1024];
		e->GetErrorMessage(szErr, 1024);
		AfxMessageBox(szErr);
		e->Delete();
	}

	if (pFile != nullptr) pFile->Close();
}
void CAISmartHomeDlg::TrainModel() {
	AfxBeginThread(TrainModelThread, this);
}

// 2개의 온도 데이터를 파이썬 서버로 전송하여 추천온도를 받아오는 함수
double CAISmartHomeDlg::SendDataToPythonServer(double outTEMP, double inTEMP) {

	if (!ConnectToServer())
	{
		return OUTPUT_ERROR;
	}

	CHttpFile* pFile = nullptr;
	double result = 0.0;

	try {
		pFile = pServer->OpenRequest(CHttpConnection::HTTP_VERB_POST, _T("/"));

		// JSON 데이터 생성
		rapidjson::StringBuffer s;
		rapidjson::Writer<rapidjson::StringBuffer> writer(s);
		writer.StartObject();
		writer.Key("outTEMP");
		writer.Double(outTEMP);	//외부온도 입력
		writer.Key("inTEMP");
		writer.Double(inTEMP);	//실내온도 입력
		writer.EndObject();
		std::string data = s.GetString();

		// HTTP 헤더 설정후 전송
		pFile->AddRequestHeaders(_T("Content-Type: application/json"));
		pFile->SendRequest(NULL, 0, (LPVOID)data.c_str(), data.size());

		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);

		// 파이썬 서버의 응답을 확인
		if (dwStatusCode == HTTP_STATUS_OK) {
			std::string responseStr;
			char buffer[4096] = { 0 };

			// 응답 데이터를 반복적으로 읽어옴
			while (pFile->Read(buffer, sizeof(buffer) - 1)) {
				buffer[sizeof(buffer) - 1] = '\0';  // null-terminate the string
				responseStr += buffer;
			}

			// 디버그 출력
			TRACE("Raw response from server: %s\n", responseStr.c_str());

			// UTF-8 문자열을 CString으로 변환
			CString utf8Response = UTF8toCString(responseStr);

			// JSON 파싱
			rapidjson::Document document;

			// 파싱 오류시 케이스
			if (document.Parse(responseStr.c_str()).HasParseError()) {
				AfxMessageBox(_T("JSON 파싱 오류가 발생했습니다."));
				result = OUTPUT_ERROR;  // 오류 처리
			}
			// 최종 성공시 실행영역
			else if (document.HasMember("result") && document["result"].IsNumber()) {
				result = document["result"].GetDouble();  // 결과 값 추출
			}
			//예상과 다른 형식일시
			else {
				AfxMessageBox(_T("응답 형식이 올바르지 않습니다."));
				result = OUTPUT_ERROR;
			}
		}
		// 서버 응답 실패시
		else {
			CString errorMsg;
			errorMsg.Format(_T("HTTP 요청 실패: 상태 코드 %lu"), dwStatusCode);
			AfxMessageBox(errorMsg);
			result = OUTPUT_ERROR;
		}
	}
	catch (CInternetException* e) {
		TCHAR szErr[1024];
		e->GetErrorMessage(szErr, 1024);
		AfxMessageBox(szErr);
		e->Delete();
		result = OUTPUT_ERROR;
	}

	if (pFile != NULL) pFile->Close();

	return result;
}

// 파이썬 서버에서 받아온 추천온도를 아두이노로 전송하는 함수
void CAISmartHomeDlg::RecommendTEMPON() {

	if (m_outTEMP == 0.0 && m_inTEMP == 0.0) {
		AfxMessageBox(_T("입력값이 없는것 같습니다."));
		return;
	}

	double result = 0.0;

	//파이썬 서버와 통신하는 함수 작동
	try {
		//'온도 받아오기' 버튼으로 인해 설정된 2개의 값을 파이썬 서버로 전송
		result = SendDataToPythonServer(m_outTEMP, m_inTEMP);
	}
	catch (const std::exception& e) {
		CString errMsg(e.what());
		AfxMessageBox(_T("예외 발생: ") + errMsg);
	}

	// 입력값과 결과를 메시지 박스로 출력
	CString strResult;
	strResult.Format(_T("외부온도: %.2f\n내부온도: %.2f\n추천온도: %.2f"), m_outTEMP, m_inTEMP, result);
	OutputDebugStringA(strResult);

	// 추천온도 아두이노 전송
	CStringA RecommendTEMP;
	RecommendTEMP.Format("RecommendTEMP,%.2f\n", result);
	const char* sendBuffer = RecommendTEMP.GetString();

	CString RR;
	RR.Format(_T("데이터 송신을 시도합니다. %s"), RecommendTEMP.GetString());
	OutputDebugStringA(RR);

	BOOL bSuccess = m_serial.WriteData(sendBuffer, RecommendTEMP.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
		OutputDebugString(_T("Data Ok\n"));
	}
	else {
		//AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
		OutputDebugString(_T("Data Fail\n"));
	}
}

// 추천온도 실행 종료 함수
void CAISmartHomeDlg::RecommendTEMPOFF()
{
	// ANSI 문자열로 변환
	CStringA RECOMMENDCommandA("RECOMMEND OFF\n"); // '\n' 추가
	const char* sendBuffer = RECOMMENDCommandA.GetString();

	//AfxMessageBox(_T("데이터 송신을 시도합니다."));

	BOOL bSuccess = m_serial.WriteData(sendBuffer, RECOMMENDCommandA.GetLength());

	if (bSuccess) {
		//AfxMessageBox(_T("데이터가 정상적으로 송신되었습니다."));
		OutputDebugString(_T("Data Ok\n"));
	}
	else {
		//AfxMessageBox(_T("데이터 송신에 실패하였습니다."));
		OutputDebugString(_T("Data Fail\n"));

	}
}

// 외부날씨 데이터를 활용하여 파이썬 서버에서 팁을 구현하고 팁을 받아와 클라이언트에 전송
void CAISmartHomeDlg::SendWeatherDataToPython(CMySocket* pClientSocket, const WeatherInfo& info, const MicroInfo& Minfo, const UVInfo& UVinfo) {
	OutputDebugString(_T("SendWeatherDataToPython called.\n"));

	if (!ConnectToServer())
	{
		OutputDebugString(_T("ConnectToServer failed.\n"));
		return;
	}

	CHttpFile* pFile = nullptr;

	try {
		pFile = pServer->OpenRequest(CHttpConnection::HTTP_VERB_POST, _T("/send_weather_data"));

		// JSON 데이터 생성
		rapidjson::StringBuffer s;
		rapidjson::Writer<rapidjson::StringBuffer> writer(s);
		writer.StartObject();
		writer.Key("sky");
		writer.String(ConvertCStringToUTF8(info.sky).c_str()); //문자열 형식을 원활히 전송하기 위해 이 부분만 다르게 설정함
		writer.Key("tmp");
		writer.String(UTF8ToCString(info.tmp));
		writer.Key("pty");
		writer.String(ConvertCStringToUTF8(info.pty).c_str());
		writer.Key("pop");
		writer.String(UTF8ToCString(info.pop));
		writer.Key("reh");
		writer.String(UTF8ToCString(info.reh));
		writer.Key("mic");
		writer.String(UTF8ToCString(Minfo.Micro));
		writer.Key("uv");
		writer.String(UTF8ToCString(UVinfo.UVdata));
		writer.EndObject();

		std::string jsonStr = s.GetString();

		// POST 요청 헤더 설정
		pFile->AddRequestHeaders(_T("Content-Type: application/json"));

		// UTF-8로 인코딩된 JSON 문자열 전송
		pFile->SendRequest(NULL, 0, (LPVOID)jsonStr.c_str(), jsonStr.size());

		// 서버 응답 읽기
		CString responseStr;
		DWORD dwStatusCode;
		pFile->QueryInfoStatusCode(dwStatusCode);
		if (dwStatusCode == HTTP_STATUS_OK) {
			CString strResponse;
			while (pFile->ReadString(strResponse)) {
				responseStr += strResponse;
			}

			// 서버 응답에서 AI의 조언 배열을 추출하고 메시지 박스로 출력
			rapidjson::Document document;
			if (document.Parse(responseStr.GetString()).HasParseError()) {
				AfxMessageBox(_T("JSON 파싱 오류"));
			}
			else if (document.HasMember("ai_advice") && document["ai_advice"].IsArray()) {
				// 배열을 순회하면서 각 항목을 출력
				const rapidjson::Value& aiArray = document["ai_advice"];
				CString adviceMessage;
				// 응답받은 AI메세지 배열을 개별적으로 전송
				for (rapidjson::SizeType i = 0; i < aiArray.Size(); ++i) {
					adviceMessage = UTF8ToCString(aiArray[i].GetString());

					// CString을 UTF-8 std::vector<char>로 변환
					std::vector<char> adviceBytes = CStringToUTF8Vector(adviceMessage);

					adviceBytes.push_back('\n'); // 필요시 개행 문자 추가
					adviceBytes.push_back('\0');  // 널 종료 문자 추가

					CStringA adviceMessage1;
					adviceMessage1.Format("RecommendTEMP,%.2f\n", adviceMessage);

					// 예: "ADVICE" 타입으로 전송
					SendToClient(pClientSocket, adviceBytes, _T("ADVICE"), pClientSocket->clientID);
				}
			}
		}

		pFile->Close();
	}
	catch (CInternetException* e) {
		TCHAR szErr[1024];
		e->GetErrorMessage(szErr, 1024);
		CString errorMsg;
		errorMsg.Format(_T("CInternetException: %s"), szErr);
		OutputDebugString(errorMsg);
		AfxMessageBox(errorMsg);
		e->Delete();
	}
}
