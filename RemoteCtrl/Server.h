#pragma once
#include<MSWSock.h>
#include "Thread.h"
#include "Queue.h"
#include "Tool.h"
#include <map>

enum EOperator {
	ENone,
	EAccept,
	ERecv,
	ESend,
	EError
};

class CServer;
class CClient;
typedef std::shared_ptr<CClient> PCLIENT;

class COverlapped {
public:
	OVERLAPPED m_overlapped;
	DWORD m_operator;//操作 参见EOperator
	std::vector<char> m_buffer;//缓冲区
	ThreadWorker m_worker;//处理函数
	CServer* m_server;//服务器对象
	CClient* m_client;//对应的客户端
	WSABUF m_wsabuffer;
	virtual ~COverlapped() {
		m_buffer.clear();
	}
};

template<EOperator>
class AcceptOverlapped :public COverlapped, ThreadFuncBase {
public:
	AcceptOverlapped();
	int AcceptWorker();
};
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;

template<EOperator>
class RecvOverlapped :public COverlapped, ThreadFuncBase {
public:
	RecvOverlapped();
	int RecvWorker();
};
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;

template<EOperator>
class SendOverlapped :public COverlapped, ThreadFuncBase {
public:
	SendOverlapped();
	int SendWorker();
};
typedef SendOverlapped<ESend> SENDOVERLAPPED;

template<EOperator>
class ErrorOverlapped :public COverlapped, ThreadFuncBase {
public:
	ErrorOverlapped() :m_operator(EError), m_worker(this, &ErrorOverlapped::ErrorWorker) {
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		m_buffer.resize(1024);
	}
	int ErrorWorker() {
		//TODO:
	}
};
typedef ErrorOverlapped<EError> ERROROVERLAPPED;

class CClient : public ThreadFuncBase {
public:
	CClient() :m_isbusy(false), m_overlapped(new ACCEPTOVERLAPPED()), m_flags(0), m_recv(new RECVOVERLAPPED()), m_send(new SENDOVERLAPPED()), 
	m_vecSend(this,(SENDCALLBACK)& CClient::SendData) {
		m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		m_buffer.resize(1024);
		memset(&m_laddr, 0, sizeof(m_laddr));
		memset(&m_raddr, 0, sizeof(m_raddr));
	}
	~CClient() {
		m_buffer.clear();
		closesocket(m_sock);
		m_recv.reset();
		m_send.reset();
		m_overlapped.reset();
		m_vecSend.Clear();
	}
	void SetOverlapped(PCLIENT& ptr) {
		m_overlapped->m_client = ptr.get();
		m_recv->m_client = ptr.get();
		m_send->m_client = ptr.get();
	}
	operator SOCKET() {
		return m_sock;
	}
	operator PVOID() {
		return &m_buffer[0];
	}
	operator LPOVERLAPPED() {
		return &m_overlapped->m_overlapped;
	}
	operator LPDWORD() {
		return &m_received;
	}
	LPWSABUF RecvWSABuffer() {
		return &m_recv->m_wsabuffer;
	}
	LPWSAOVERLAPPED RecvOverlapped() {
		return &m_recv->m_overlapped;
	}
	LPWSABUF SendWSABuffer() {
		return &m_send->m_wsabuffer;
	}
	LPWSAOVERLAPPED SendOverlapped() {
		return &m_send->m_overlapped;
	}
	DWORD& flags() { return m_flags; }
	sockaddr_in* GetLocalAddr() { return &m_laddr; }
	sockaddr_in* GetRemoteAddr() { return &m_raddr; }
	size_t GetBufferSize() const { return m_buffer.size(); }
	int Recv() {
		int ret = recv(m_sock, m_buffer.data() + m_used, m_buffer.size() - m_used, 0);
		if (ret <= 0) return -1;
		m_used += (size_t)ret;
		CTool::Dump((BYTE*)m_buffer.data(), ret);
		return 0;
	}
	int Send(void* buffer, size_t nSize) {
		std::vector<char> data(nSize);
		memcpy(data.data(), buffer, nSize);
		if (m_vecSend.PushBack(data)) {
			return 0;
		}
		return -1;
	}
	int SendData(std::vector<char>& data) {
		if (m_vecSend.Size() > 0) {
			int ret = WSASend(m_sock, SendWSABuffer(), 1, &m_received, m_flags, &m_send->m_overlapped, NULL);
			if (ret != 0 && WSAGetLastError() != WSA_IO_PENDING) {
				CTool::ShowError();
				return -1;
			}
		}
		return 0;
	}
private:
	SOCKET m_sock;
	DWORD m_received;
	DWORD m_flags;
	std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;
	std::shared_ptr<RECVOVERLAPPED> m_recv;
	std::shared_ptr<SENDOVERLAPPED> m_send;
	std::vector<char> m_buffer;
	size_t m_used;//已经使用的缓冲区大小
	sockaddr_in m_laddr;//本地地址
	sockaddr_in m_raddr;//远程地址
	bool m_isbusy;
	CSendQueue<std::vector<char>> m_vecSend;//发送数据队列
};

class CServer :
	public ThreadFuncBase
{
public:
	CServer(const std::string& ip = "0.0.0.0", short port = 9527) :m_pool(10) {
		m_hIOCP = INVALID_HANDLE_VALUE;
		m_sock = INVALID_SOCKET;
		m_addr.sin_family = AF_INET;
		m_addr.sin_port = htons(port);
		m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
	}
	~CServer() {
		closesocket(m_sock);
		std::map<SOCKET, PCLIENT>::iterator it = m_client.begin();
		for (; it != m_client.end(); it++) {
			it->second.reset();
		}
		m_client.clear();
		CloseHandle(m_hIOCP);
		m_pool.Stop();
		WSACleanup();
	}
	bool StartService() {
		CreateSocket();
		if (bind(m_sock, (sockaddr*)&m_addr, sizeof(m_addr)) == -1) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			return false;
		}
		if (listen(m_sock, 3) == -1) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			return false;
		}
		m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);
		if (m_hIOCP == NULL) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			m_hIOCP = INVALID_HANDLE_VALUE;
			return false;
		}
		CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);
		m_pool.Invoke();
		m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&CServer::threadIocp));
		if (!NewAccept()) return false;
		//m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&CServer::threadIocp));
		//m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&CServer::threadIocp));
		return true;
	}
	bool NewAccept() {
		PCLIENT pClient(new CClient());
		pClient->SetOverlapped(pClient);
		m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));
		if (!AcceptEx(m_sock, *pClient, *pClient, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, *pClient, *pClient)) {
			TRACE("%d\r\n", WSAGetLastError());
			if (WSAGetLastError() != WSA_IO_PENDING) {
				closesocket(m_sock);
				m_sock = INVALID_SOCKET;
				m_hIOCP = INVALID_HANDLE_VALUE;
				return false;
			}
		}
		return true;
	}
	void BindNewSocket(SOCKET s) {
		CreateIoCompletionPort((HANDLE)s, m_hIOCP, (ULONG_PTR)this, 0);
	}
private:
	void CreateSocket() {
		WSADATA WSAData;
		WSAStartup(MAKEWORD(2, 2), &WSAData);
		m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		int opt = 1;
		setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
	}
	int threadIocp() {
		DWORD transferred = 0;
		ULONG_PTR CompletionKey = 0;
		OVERLAPPED* lpOverlapped = NULL;
		if (GetQueuedCompletionStatus(m_hIOCP, &transferred, &CompletionKey, &lpOverlapped, INFINITE)) {
			if (CompletionKey != 0) {
				COverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, COverlapped, m_overlapped);
				TRACE("pOverlapped->m_operator %d \r\n", pOverlapped->m_operator);
				pOverlapped->m_server = this;
				switch (pOverlapped->m_operator) {
				case EAccept:
				{
					ACCEPTOVERLAPPED* pOver = (ACCEPTOVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pOver->m_worker);
				}
				break;
				case ERecv:
				{
					RECVOVERLAPPED* pOver = (RECVOVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pOver->m_worker);
				}
				break;
				case ESend:
				{
					SENDOVERLAPPED* pOver = (SENDOVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pOver->m_worker);
				}
				break;
				case EError:
				{
					ERROROVERLAPPED* pOver = (ERROROVERLAPPED*)pOverlapped;
					m_pool.DispatchWorker(pOver->m_worker);
				}
				break;
				}
			}
			else {
				return -1;
			}
		}
		return 0;
	}
private:
	CThreadPool m_pool;
	HANDLE m_hIOCP;
	SOCKET m_sock;
	sockaddr_in m_addr;
	std::map<SOCKET, std::shared_ptr <CClient>>m_client;
};

