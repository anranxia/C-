#pragma once
#include"pch.h"
#include<atomic>
template<class T>
class CQueue
{//线程安全的队列（利用IOCP实现）
public:
	enum {
		CQNone,
		CQPush,
		CQPop,
		CQSize,
		CQClear
	};
	typedef struct IocpParam {
		size_t nOperator;//操作
		T Data;//数据
		HANDLE hEvent;//pop操作需要的
		IocpParam(int op, const T& data, HANDLE hEve = NULL) {
			nOperator = op;
			Data = data;
			hEvent = hEve;
		}
		IocpParam() {
			nOperator = CQNone;
		}
	}PPARAM;//Post Parameter 用于投递信息的结构体
public:
	CQueue() {
		m_lock = false;
		m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
		m_hThread = INVALID_HANDLE_VALUE;
		if (m_hCompletionPort != NULL) {
			HANDLE hThread = (HANDLE)_beginthread(&CQueue<T>::threadEntry, 0, m_hCompletionPort);
		}
	}
	~CQueue() {
		if (m_lock) return;
		m_lock = true;
		HANDLE hTemp = m_hCompletionPort;
		PostQueuedCompletionStatus(m_hCompletionPort, 0, NULL, NULL);
		WaitForSingleObject(m_hThread, INFINITE);
		m_hCompletionPort = NULL;
		CloseHandle(hTemp);
	}
	bool PushBack(const T& data) {
		if (m_lock == true) return false;
		IocpParam* pParam = new IocpParam(CQPush, data);
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (ret == false)delete pData;
		return ret;
	}
	bool PopFront(T& data) {
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam Param(CQPop, data, hEvent);
		if (m_lock)
		{
			if (hEvent) CloseHandle(hEvent);
			return false;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		if (ret == false)
		{
			CloseHandle(hEvent);
			return false;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			data = Param.Data;
		}
		return ret;
	}
	size_t Size() {
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam Param(CQSize, T(), hEvent);
		if (m_lock)
		{
			if (hEvent) CloseHandle(hEvent);
			return -1;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		if (ret == false)
		{
			CloseHandle(hEvent);
			return -1;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			return Param.nOperator;
		}
		return -1;
	}
	void Clear() {
		if (m_lock == true) return false;
		IocpParam* pParam = new IocpParam(CQClear, T());
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (ret == false)delete pData;
		return ret;
	}
private:
	static void threadEntry(void* arg) {
		CQueue<T>* thiz = (CQueue<T>*)arg;
		thiz->threadMain();
		_endthread();
	}
	void threadMain() {
		PPARAM* pParam = NULL;
		DWORD dwTransferred = 0;
		ULONG_PTR CompletionKey = 0;
		OVERLAPPED* pOverlapped = NULL;
		while (GetQueuedCompletionStatus(m_hCompletionPort, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE)) {
			if (dwTransferred == 0 || CompletionKey == NULL) {
				printf("thread is prepare to exit!\r\n");
				break;
			}
			pParam = (IOCP_PARAM*)CompletionKey;
			switch (pParam->nOperator) {
			case CQPush:
				m_lstData.push_back(pParam->strData);
				delete pParam;
				break;
			case CQPop:
				if (m_lstData.size() > 0) {
					pParam->Data = m_lstData.front();
					m_lstData.pop_front();
				}
				if (pParam->hEvent != NULL) {
					SetEvent(pParam->hEvent);
				}
				break;
			case CQSize:
				pParam->nOperator = m_lstData.size();
				if (pParam->hEvent != NULL) {
					SetEvent(pParam->hEvent);
				}
				break;
			case CQClear:
				m_lstData.clear();
				delete pParam;
				break;
			default:
				OutputDebugString("unknown operator!\r\n");
				break;
			}
		}
		CloseHandle(m_hCompletionPort);
	}
private:
	std::list<T> m_lstData;
	HANDLE m_hCompletionPort;
	HANDLE m_hThread;
	std::atomic<bool> m_lock;//队列正在析构
};

