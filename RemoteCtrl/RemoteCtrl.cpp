// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include "Command.h"
#include <conio.h>
#include "Queue.h"
#include <MSWSock.h>
#include "Server.cpp"
#include "ENetwork.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 唯一的应用程序对象

//#define INVOKE_PATH _T("C:\\Windows\\SysWOW64\\RemoteCtrl.exe")
#define INVOKE_PATH _T("C:\\Users\\99597\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl.exe")

CWinApp theApp;

using namespace std;

//业务和通用

bool ChooseAutoInvoke(const CString& strPath) {
	TCHAR wcsSystem[MAX_PATH] = _T("");
	if (PathFileExists(strPath)) {
		return true;
	}
	CString strInfo = _T("该程序只允许用于合法的用途！\n");
	strInfo += _T("继续运行该程序，将使得这台机器处于被监控状态！\n");
	strInfo += _T("如果你不希望这样，请按“取消”按钮，退出程序。\n");
	strInfo += _T("按下“是”按钮，该程序将被复制到你的机器上，并随系统启动而自动运行！\n");
	strInfo += _T("按下“否”按钮，程序只运行一次，不会在系统内留下任何东西！\n");
	int ret = MessageBox(NULL, strInfo, _T("警告"), MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);
	if (ret == IDYES) {
		//WriteRegisterTable(strPath);
		if (!CTool::WriteStartupDir(strPath)) {
			MessageBox(NULL, _T("复制文件失败，是否权限补不足？\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
			return false;
		}
	}
	else if (ret == IDCANCEL) {
		return false;
	}
	return true;
}

/*
1 易用性
	a 简化参数
	b 类型适配（参数适配）
	c 流程简化
2 易移植性（高内聚，低耦合）
	a 核心功能到底是什么？
	b 业务逻辑是什么？
*/

void iocp() {
	CServer server;
	server.StartService();
	getchar();
}

void initsock() {
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
}
void clearsock() {
	WSACleanup();
}
int RecvFromCB(void* arg, const EBuffer& buffer, ESockaddrIn& addr) {
	EServer* server = (EServer*)arg;
	return server->Sendto(addr, buffer);
}
int SendToCB(void* arg, const ESockaddrIn& addr, int ret) {
	EServer* server = (EServer*)arg;
	printf("sendto done!%p\r\n", server);
	return 0;
}
void udp_server() {
	std::list<ESockaddrIn> lstclients;
	printf("%s(%d):%s\r\n", __FILE__, __LINE__, __FUNCTION__);
	EServerParameter param("127.0.0.1", 20000, ETYPE::ETypeUDP, NULL, NULL, NULL, RecvFromCB, SendToCB);
	EServer server(param);
	server.Invoke(&server);
	printf("%s(%d):%s\r\n", __FILE__, __LINE__, __FUNCTION__);
	getchar();
	return;

}
void udp_client(bool ishost = true) {
	Sleep(2000);
	sockaddr_in server, client;
	int len = sizeof(client);
	server.sin_family = AF_INET;
	server.sin_port = htons(20000);
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	SOCKET sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		printf("%s(%d):%s ERROR!!!\r\n", __FILE__, __LINE__, __FUNCTION__);
		return;
	}
	if (ishost) {//主客户端代码
		printf("%s(%d):%s\r\n", __FILE__, __LINE__, __FUNCTION__);
		std::string msg = "hello world!\n";
		int ret = sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&server, sizeof(server));
		printf("%s(%d):%s ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
		if (ret > 0) {
			msg.resize(1024);
			memset((char*)msg.c_str(), 0, msg.size());
			ret = recvfrom(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)&client, &len);
			printf("host %s(%d):%s ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
			if (ret > 0) {
				printf("%s(%d):%s ip %08X port %d\r\n", __FILE__, __LINE__, __FUNCTION__, client.sin_addr.s_addr, ntohs(client.sin_port));
				printf("%s(%d):%s msg = %d\r\n", __FILE__, __LINE__, __FUNCTION__, msg.size());
			}
			ret = recvfrom(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)&client, &len);
			printf("host after recv from client %s(%d):%s ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
			if (ret > 0) {
				printf("%s(%d):%s ip %08X port %d\r\n", __FILE__, __LINE__, __FUNCTION__, client.sin_addr.s_addr, ntohs(client.sin_port));
				printf("%s(%d):%s msg = %s\r\n", __FILE__, __LINE__, __FUNCTION__, msg.c_str());
			}
		}
	}
	else {//从客户端代码
		printf("%s(%d):%s\r\n", __FILE__, __LINE__, __FUNCTION__);
		std::string msg = "hello world!\n";
		int ret = sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&server, sizeof(server));
		printf("%s(%d):%s ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
		if (ret > 0) {
			msg.resize(1024);
			memset((char*)msg.c_str(), 0, msg.size());
			ret = recvfrom(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)&client, &len);
			printf("client %s(%d):%s ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
			if (ret > 0) {
				sockaddr_in addr;
				memcpy(&addr, msg.c_str(), sizeof(addr));
				sockaddr_in* paddr = (sockaddr_in*)&addr;
				printf("%s(%d):%s ip %08X port %d\r\n", __FILE__, __LINE__, __FUNCTION__, client.sin_addr.s_addr, ntohs(client.sin_port));
				printf("%s(%d):%s msg = %d\r\n", __FILE__, __LINE__, __FUNCTION__, msg.size());
				printf("%s(%d):%s ip %08X port %d\r\n", __FILE__, __LINE__, __FUNCTION__, paddr->sin_addr.s_addr, ntohs(paddr->sin_port));
				msg = "Hello,I am client!\r\n";
				ret = sendto(sock, (char*)msg.c_str(), msg.size(), 0, (sockaddr*)paddr, sizeof(sockaddr_in));
				printf("client after send to host %s(%d):%s ret = %d\r\n", __FILE__, __LINE__, __FUNCTION__, ret);
			}
		}
	}
	closesocket(sock);
}


//int main()
//{
//	if (!CTool::Init()) return 1;
//
//	//iocp();②
//
//	//if (CTool::IsAdmin()) {//①
//	//	if (!CTool::Init()) return 1;
//	//	if (ChooseAutoInvoke(INVOKE_PATH)) {
//	//		CCommand cmd;
//	//		int ret = CServerSocket::getInstance()->Run(&CCommand::RunCommand, &cmd);
//	//		switch (ret) {
//	//		case -1:
//	//			MessageBox(NULL, _T("网络初始化异常，未能成功初始化，请检查网络状态！"), _T("网络初始化失败！"), MB_OK | MB_ICONERROR);
//	//			break;
//	//		case -2:
//	//			MessageBox(NULL, _T("多次无法正常接入用户，结束程序！"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
//	//			break;
//	//		}
//	//	}
//	//}
//	//else {
//	//	if (CTool::RunAsAdmin() == false) {
//	//		CTool::ShowError();
//	//		return 1;
//	//	}
//	//}
//	return 0;
//}

//int wmain(int argc, TCHAR* argv[]);
//int _tmain(int argc, TCHAR* argv[]);
int main(int argc, char* argv[]) {
	if (!CTool::Init()) return 1;
	initsock();
	if (argc == 1) {
		char wstrDir[MAX_PATH];
		GetCurrentDirectoryA(MAX_PATH, wstrDir);
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		memset(&si, 0, sizeof(si));
		memset(&pi, 0, sizeof(pi));
		string strCmd = argv[0];
		strCmd += " 1";
		BOOL bRet = CreateProcessA(NULL, (LPSTR)strCmd.c_str(), NULL, NULL, FALSE, 0, NULL, wstrDir, &si, &pi);
		if (bRet) {
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			TRACE("进程ID:%d\r\n", pi.dwProcessId);
			TRACE("进程ID:%d\r\n", pi.dwThreadId);
			strCmd += " 2";
			BOOL bRet = CreateProcessA(NULL, (LPSTR)strCmd.c_str(), NULL, NULL, FALSE, 0, NULL, wstrDir, &si, &pi);
			if (bRet) {
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
				TRACE("进程ID:%d\r\n", pi.dwProcessId);
				TRACE("进程ID:%d\r\n", pi.dwThreadId);
				udp_server();//服务器代码
			}
		}
	}
	else if (argc == 2) {//主客户端
		udp_client();
	}
	else {//从客户端
		Sleep(2000);
		udp_client(false);
	}
	clearsock();
	return 0;
}