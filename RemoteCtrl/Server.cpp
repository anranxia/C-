#include "pch.h"
#include "Server.h"
#pragma warning(disable:4407)
template<EOperator op>
AcceptOverlapped<op>::AcceptOverlapped(){
	m_worker = ThreadWorker(this, (FUNCTYPE)&AcceptOverlapped<op>::AcceptWorker);
	m_operator = EAccept;
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024);
	m_server = NULL;
}

template<EOperator op>
int AcceptOverlapped<op>::AcceptWorker(){
	INT lLength = 0, rLength = 0;
	if (m_client->GetBufferSize() > 0) {
		sockaddr* plocal = NULL, * premote = NULL;
		GetAcceptExSockaddrs(*m_client, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
			(sockaddr**)&plocal, &lLength, (sockaddr**)&premote, &rLength);
		memcpy(m_client->GetLocalAddr(), plocal, sizeof(sockaddr_in));
		memcpy(m_client->GetRemoteAddr(), premote, sizeof(sockaddr_in));
		m_server->BindNewSocket(*m_client);
		int ret = WSARecv((SOCKET)*m_client, m_client->RecvWSABuffer(), 1, *m_client, &m_client->flags(), m_client->RecvOverlapped(), NULL);
		if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
			//TODO:报错
			TRACE("ret = %d error = %d\r\n", ret, WSAGetLastError());
		}
		if (!m_server->NewAccept()) {
			return -2;
		}
	}
	return -1;
}

template<EOperator op>
RecvOverlapped<op>::RecvOverlapped() {
	m_worker = ThreadWorker(this, (FUNCTYPE)&RecvOverlapped<op>::RecvWorker);
	m_operator = ERecv;
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024 * 256);
}

template<EOperator op>
int RecvOverlapped<op>::RecvWorker(){
	int ret = m_client->Recv();
	return ret;
}

template<EOperator op>
SendOverlapped<op>::SendOverlapped() {
	m_worker = ThreadWorker(this, (FUNCTYPE)&SendOverlapped<op>::SendWorker);
	m_operator = ESend;
	memset(&m_overlapped, 0, sizeof(m_overlapped));
	m_buffer.resize(1024 * 256);
}

template<EOperator op>
int SendOverlapped<op>::SendWorker() {
	//TODO:
	/*
	* 1 Send可能不会立即完成
	* 
	*/
	return -1;
}
