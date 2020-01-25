#include "servertool.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
	LONG changed;
	INT stage;
	BOOL IsResending;
} CONNSTATUS_T;

static ULONG GetTimeNow()
{
	return (LONG)time(NULL);
}

VOID FormatRequest(LPVOID lpBuf,NETADR_T* pAddr,LPSTR lpSearch,PSIZE_T pszLen)
{
	PBYTE pCur;
	SIZE_T szLen;
	PCHAR pChar;
	CHAR szFormat[32];
	
	pCur = (PBYTE)lpBuf;
	*pCur++ = 0x31;
	*pCur++ = 0xFF;
	
	sprintf(szFormat,"%s:%d",inet_ntoa(pAddr->sin_addr),
		ntohs(pAddr->sin_port));
	pChar = szFormat;
	szLen = strlen(pChar)+1;
	CopyMemory(pCur,pChar,szLen);
	pCur += szLen;

	pChar = lpSearch;
	szLen = strlen(pChar)+1;
	CopyMemory(pCur,pChar,szLen);
	pCur += szLen;

	*pszLen = (SIZE_T)((char*)pCur-(char*)lpBuf);
}

VOID MakeNonblocking(SOCKET s)
{
	unsigned long arg = 1;
	ioctlsocket(s, FIONBIO, &arg);
}

BOOL FindMasterServers(MASTERFINDPARAMETERS_T* pParams)
{
	struct hostent* host;
	NETADR_T addr;
	
	CONNECTION_T m_conn[32];
	INT m_num;
	INT stages[32];
	INT rel[32];
	INT changed,tries;
	
	WSAPOLLFD polls[32];
	INT p_num;
	
	static BYTE szPacket[1240];
	SIZE_T szLen;
	INT i;

	host = gethostbyname("hl2master.steampowered.com");
	if(!host) return FALSE;

	addr.sin_addr.s_addr = 0;
	addr.sin_port = htons(0);

	FormatRequest(szPacket,&addr,"\\app\\4020",&szLen);

	i = 0;
	m_num = 0;
	while(host->h_addr_list[i] != 0)
	{
		INT j;
		
		for(j = 27010; j <= 27015; j++)
		{
			m_conn[m_num].s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

			m_conn[m_num].addr.sin_family = AF_INET;
			m_conn[m_num].addr.sin_addr = *(IN_ADDR*)host->h_addr_list[i];
			m_conn[m_num].addr.sin_port = htons(j);
			m_num++;
		}

		i++;
	}
	
	ZeroMemory(stages,sizeof(stages));

	tries = 0;
	do {
		p_num = 0;
		for(i = 0; i < m_num; i++)
		{
			if(stages[i] == 2) continue;
			polls[p_num].fd = m_conn[i].s;
			if(stages[i] == 0)
				polls[p_num].events = POLLOUT;
			else polls[p_num].events = POLLIN;
			rel[p_num] = i;
			p_num++;
		}

		changed = 0;

		if(p_num == 0) break;
		if(WSAPoll(polls,p_num,500) > 0) //5 second timeout
		{
			for(i = 0; i < p_num; i++)
			{
				CONNECTION_T* pConn;

				pConn = &m_conn[rel[i]];
				if(polls[i].revents & POLLOUT)
				{
					sendto(pConn->s,(const char*)szPacket,(INT)szLen,0,
						(const struct sockaddr*)&pConn->addr,sizeof(struct sockaddr_in));
					stages[rel[i]]++;
					//printf("sent request to %s:%d\n",inet_ntoa(pConn->addr.sin_addr),
					//	ntohs(pConn->addr.sin_port));
					changed = 1;
				}
				else if(polls[i].revents & POLLIN)
				{
					INT FromLen;

					FromLen = sizeof(struct sockaddr_in);
					recvfrom(pConn->s,(char*)szPacket,sizeof(szPacket),0,
						(struct sockaddr*)&pConn->addr,&FromLen);
					stages[rel[i]]++;

					//Close
					closesocket(pConn->s);

					//Add to list
					CopyMemory(pParams->addrs,&pConn->addr,sizeof(struct sockaddr_in));
					pParams->addrs++;

					pParams->AddServer(&pConn->addr);

					//printf("recv response from %s:%d\n",inet_ntoa(pConn->addr.sin_addr),
					//	ntohs(pConn->addr.sin_port));
					changed = 1;
				}
				//unnecessary for UDP
				else if(polls[i].revents & (POLLHUP|POLLERR))
				{
					//Error

					//Close
					closesocket(pConn->s);
					stages[rel[i]] = 2;

					//printf("error %s:%d\n",inet_ntoa(pConn->addr.sin_addr),
					//	ntohs(pConn->addr.sin_port));
				}
			}
		}

		//printf("changed %d tries %d\n",changed,tries);
		if(changed == 0)
		{
			if(tries++ == 2)
			{
				for(i = 0; i < m_num; i++)
				{
					if(stages[i] == 2) continue;
					stages[i] = 2;
					closesocket(m_conn[i].s);
				}
			}
		} else tries = 0;
	} while(p_num > 0);

	return TRUE;
}

INT DoMasterServers(MASTER_T* Masters,INT MasterNum,LPCSTR lpSearch)
{
	static BYTE szPacket[2048];
	CONNSTATUS_T status[32];
	WSAPOLLFD polls[32];
	SIZE_T szLen;
	INT rel[32];
	INT i,p_num,CurTime,Delta;
	INT servers;
	PBYTE pPtr;

	for(i = 0; i < MasterNum; i++)
	{
		Masters[i].Conn.s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
		Masters[i].ServerNum = 0;
		Masters[i].PacketNum = 0;
		MakeNonblocking(Masters[i].Conn.s);
		status[i].stage = 0;
		status[i].changed = GetTimeNow();
	}

	servers = 0;
	do {
		p_num = 0;
		for(i = 0; i < MasterNum; i++)
		{
			if(status[i].stage == 2)
				continue;
			polls[p_num].fd = Masters[i].Conn.s;
			if(status[i].stage == 0)
				polls[p_num].events = POLLOUT;
			else polls[p_num].events = POLLIN;
			rel[p_num] = i;
			p_num++;
		}

		if(p_num == 0) break;

		if(WSAPoll(polls,p_num,500))
		{
			for(i = 0; i < p_num; i++)
			{
				MASTER_T* Master;

				Master = &Masters[rel[i]];
				if(polls[i].revents & POLLOUT)
				{
					NETADR_T last;
					if(Master->ServerNum == 0)
					{
						last.sin_addr.s_addr = 0;
						last.sin_port = 0;
					}
					else last = Master->Servers[Master->ServerNum-1];
					FormatRequest(szPacket,&last,(LPSTR)lpSearch,&szLen);

					sendto(Master->Conn.s,(const char*)szPacket,(INT)szLen,0,
						(const struct sockaddr*)&Master->Conn.addr,
						sizeof(struct sockaddr_in));
					status[rel[i]].stage = 1; //Request stage
					status[rel[i]].changed = GetTimeNow();
				}
				else if(polls[i].revents & POLLIN)
				{
					INT FromLen;
					INT SvNum;

					FromLen = sizeof(struct sockaddr_in);
					szLen = recvfrom(Master->Conn.s,(char*)szPacket,sizeof(szPacket),
						0,(struct sockaddr*)&Master->Conn.addr,&FromLen);
					//Now we need to process data
					pPtr = szPacket;
					if(Master->PacketNum == 0)
					{
						//First packet have header which we want to skip
						szLen -= 6;
						MoveMemory(szPacket,szPacket+6,szLen);
					}

					SvNum = (INT)(szLen/6);
					//Check for ending mark
					if(((NETADR_T*)szPacket+SvNum-1)->sin_addr.s_addr == 0)
					{
						//printf("got last packet\n");
						SvNum--; //We don't want to copy ending mark
						//status[rel[i]].stage = 2;
						status[rel[i]].stage = 2;
						if(Master->Conn.s)
						{
							closesocket(Master->Conn.s);
							Master->Conn.s = 0;
						}
						continue;
					} else status[rel[i]].stage = 0;
					if(SvNum == 0) continue;

					//Alocate space for server data and copy server
					if(Master->ServerNum == 0)
						Master->Servers = (NETADR_T*)malloc(SvNum*6);
					else
					{
						Master->Servers = (NETADR_T*)realloc(
							Master->Servers,(Master->ServerNum*6)+SvNum*6);
					}

					CopyMemory(Master->Servers+Master->ServerNum,szPacket,SvNum*6);
					Master->ServerNum += SvNum;

					//printf("got %d servers from %s:%d\n",SvNum,inet_ntoa(pMaster->Conn.addr.sin_addr),
					//	ntohs(pMaster->Conn.addr.sin_port));
					servers += SvNum;
					status[rel[i]].changed = GetTimeNow();
				}
				else if(polls[i].revents & (POLLHUP|POLLERR))
				{
					status[rel[i]].stage = 2;
					if(Master->Conn.s)
					{
						closesocket(Master->Conn.s);
						Master->Conn.s = 0;
					}

					//printf("POLLHUP|POLLERR\n");
				}
			}
		}
		CurTime = GetTimeNow();
		for(i = 0; i < MasterNum; i++)
		{
			CONNSTATUS_T* pStat;

			pStat = &status[i];
			Delta = CurTime - pStat->changed;
			if(Delta > 8)
			{
				//Get the fuck out!
				pStat->stage = 2;
				if(Masters[i].Conn.s)
				{
					closesocket(Masters[i].Conn.s);
					Masters[i].Conn.s = 0;
				}
			}
		}
	} while(p_num != 0);
	
	return servers;
}

VOID MasterFree(MASTER_T* Master)
{
	if(Master->ServerNum)
		free(Master->Servers);
}

BOOL IsServerAlreadyInList(NETADR_T* Servers,INT ServerNum,NETADR_T* Target)
{
	INT i;

	for(i = 0; i < ServerNum; i++)
	{
		if(!memcmp(Target,&Servers[i],sizeof(NETADR_T)))
			return TRUE;
	}
	return FALSE;
}

NETADR_T* GetUniqueServers(MASTER_T* pMasters,INT MasterNum,INT* pServerNum)
{
	INT i,j;
	INT SvNum;
	NETADR_T* pServers;

	SvNum = 0;
	pServers = NULL;
	for(i = 0; i < MasterNum; i++)
	{
		for(j = 0; j < pMasters[i].ServerNum; j++)
		{
			if(!IsServerAlreadyInList(pServers,SvNum,&pMasters[i].Servers[j]))
			{
				if(SvNum == 0)
				{
					pServers = (NETADR_T*)malloc(sizeof(NETADR_T));
				}
				else
				{
					pServers = (NETADR_T*)realloc(pServers,sizeof(NETADR_T)*(SvNum+1));
				}
				CopyMemory(&pServers[SvNum++],&pMasters[i].Servers[j],sizeof(NETADR_T));
			}
		}
	}
	
	*pServerNum = SvNum;
	return pServers;
}

VOID ParsePacket(PBYTE pPacket,SERVER_T* Server)
{
	PBYTE pCur;
	INT iNum;

	pCur = pPacket;

	if(*(PINT)pCur != -1)
	{
		wcscpy(Server->szName,L"!!<wrong header>!!");
		return;
	}

	pCur += 5;
	pCur++;
	MultiByteToWideChar(CP_UTF8,0,(LPCSTR)pCur,-1,Server->szName,MAX_PATH);
	pCur += strlen((const char*)pCur)+1;
	
	MultiByteToWideChar(CP_UTF8,0,(LPCSTR)pCur,-1,Server->szMap,64);
	pCur += strlen((const char*)pCur)+1;

	pCur += strlen((const char*)pCur)+1; //Skip folder

	MultiByteToWideChar(CP_UTF8,0,(LPCSTR)pCur,-1,Server->szGame,64);
	pCur += strlen((const char*)pCur)+1;

	pCur += 2; //Skip ID

	Server->cPlayers = *pCur++;
	Server->cMaxPlayers = *pCur++;
	iNum = *pCur++;
	Server->cPlayers += iNum;
	Server->cType = *pCur++;
	Server->cOS = *pCur++;
	Server->cVisibility = *pCur++;
	Server->cVAC = *pCur++;
}

static BYTE szSourceQuery[] = "\xFF\xFF\xFF\xFFTSource Engine Query";

VOID QueryServers(NETADR_T* Addrs,SERVER_T* Servers,INT ServerNum,
	AddServer_t AddServer,INT WaitTime)
{
	static WSAPOLLFD polls[SERVER_BLOCK];
	static CONNECTION_T m_conn[SERVER_BLOCK];
	static CONNSTATUS_T status[SERVER_BLOCK];
	static INT rel[SERVER_BLOCK];
	BYTE szPacket[1240];
	INT i,p_num;
	LONG CurTime,Delta;

	ZeroMemory(status,sizeof(status));
	ZeroMemory(rel,sizeof(rel));

	for(i = 0; i < ServerNum; i++)
	{
		m_conn[i].s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
		MakeNonblocking(m_conn[i].s);
		m_conn[i].addr.sin_family = AF_INET;
		m_conn[i].addr.sin_addr = Addrs[i].sin_addr;
		m_conn[i].addr.sin_port = Addrs[i].sin_port;
		status[i].stage = 0;
		status[i].changed = GetTimeNow();
	}

	do {
		p_num = 0;
		for(i = 0; i < ServerNum; i++)
		{
			if(status[i].stage == 2) continue;
			polls[p_num].fd = m_conn[i].s;
			if(status[i].stage == 0)
				polls[p_num].events = POLLOUT;
			else polls[p_num].events = POLLIN;
			rel[p_num] = i;
			p_num++;
		}

		if(p_num == 0) break;
		
		//We need to be precisous,
		//because everything based on timings
		if(WSAPoll(polls,p_num,1000))
		{
			for(i = 0; i < p_num; i++)
			{
				CONNECTION_T* pConn;
				CONNSTATUS_T* pStat;

				pConn = &m_conn[rel[i]];
				pStat = &status[rel[i]];
				if(polls[i].revents & POLLOUT)
				{
					//Send request
					sendto(pConn->s,(const char*)szSourceQuery,25,
						0,(const struct sockaddr*)&pConn->addr,sizeof(struct sockaddr_in));
					pStat->stage = 1;
					//pStat->changed = GetTimeNow();
				}
				else if(polls[i].revents & POLLIN)
				{
					INT FromLen;

					FromLen = sizeof(struct sockaddr_in);
					recvfrom(pConn->s,(char*)szPacket,sizeof(szPacket),0,
						(struct sockaddr*)&pConn->addr,&FromLen);
					//Parse packet
					ZeroMemory(Servers,sizeof(SERVER_T));
					Servers->Addr.sin_addr = pConn->addr.sin_addr;
					Servers->Addr.sin_port = pConn->addr.sin_port;

					ParsePacket(szPacket,Servers);
					Servers->IsValid = TRUE;
					AddServer(Servers);
					Servers++;

					closesocket(pConn->s);
					pConn->s = 0;
					pStat->stage = 2;
					pStat->changed = GetTimeNow();
				}
				else if(polls[i].revents & (POLLHUP|POLLERR))
				{
					closesocket(pConn->s);
					pConn->s = 0;
					pStat->stage = 2;
				}
			}
		}
		CurTime = GetTimeNow();
		for(i = 0; i < ServerNum; i++)
		{
			if(status[i].stage == 2) continue;

			Delta = CurTime - status[i].changed;
			if(Delta >= WaitTime)
			{
				//2 seconds no activity - get the fuck out!
				status[i].stage = 2;
				if(m_conn[i].s)
				{
					closesocket(m_conn[i].s);
					m_conn[i].s = 0;
				}
			}
		}
	} while(p_num != 0);
}

static BOOL RecvTimeOut(CONNECTION_T* pConn,PBYTE pPacket,INT Len,INT TimeOut)
{
	INT FromLen;
	FromLen = sizeof(struct sockaddr_in);
	recvfrom(pConn->s,(char*)pPacket,Len,0,
		(struct sockaddr*)&pConn->addr,&FromLen);
	return TRUE;
}

static VOID ParseRules(PBYTE pMem,AddRule_t AddRule)
{
	PBYTE pPtr;
	SHORT NumRules;
	INT Len;

	pPtr = pMem;
	pPtr += 5;
	
	NumRules = *(PSHORT)pPtr;
	pPtr += 2;

	while(NumRules--)
	{
		Len = (INT)strlen((const char*)pPtr)+1;
		AddRule((PCHAR)pPtr,(PCHAR)pPtr+Len);
		pPtr += Len;
		pPtr += strlen((const char*)pPtr)+1;
	}
}

static BOOL GetServerRules(CONNECTION_T* pConn,AddRule_t AddRule)
{
	static BYTE szPacket[2048];
	WSAPOLLFD poll;
	PBYTE pMem;
	PBYTE pPtr;
	INT Len;
	INT Stage;
	UINT Challenge;
	BOOL IsMultipacket;
	INT Total,Current;
	SHORT SwitchSize;

	//Stage 0 - send request with challenge
	//Stage 1 - if challenge is -1, request again with challenge
	//	otherwise, start parsing multipackets
	//Stage 2 - final

	Total = 0;
	Current = 0;
	SwitchSize = 0;
	
	pMem = NULL;

	Challenge = (UINT)-1;
	Stage = 0;
	do {
		poll.fd = pConn->s;
		if(Stage == 0)
			poll.events = POLLOUT;
		else poll.events = POLLIN;
		
		if(WSAPoll(&poll,1,3000))
		{
			if(poll.revents & POLLOUT)
			{
				pPtr = szPacket;
				*(PUINT)pPtr = (UINT)-1;
				pPtr += 4;

				*pPtr++ = 0x56;

				*(PUINT)pPtr = (UINT)Challenge;
				pPtr += 4;

				sendto(pConn->s,(const char*)szPacket,9,0,
					(const struct sockaddr*)&pConn->addr,sizeof(struct sockaddr_in));
				Stage = 1;
			}
			else if(poll.revents & POLLIN)
			{
				INT FromLen;

				pPtr = szPacket;

				FromLen = sizeof(struct sockaddr_in);
				Len = recvfrom(pConn->s,(char*)szPacket,sizeof(szPacket),0,
					(struct sockaddr*)&pConn->addr,&FromLen);

				if(*(PUINT)pPtr == -1)
				{
					if(Challenge == -1)
					{
						pPtr += 5;
						Challenge = *(PUINT)pPtr;
						Stage = 0; //Request again
					}
					else //If it regular packet
					{
						IsMultipacket = FALSE;
						pMem = (PBYTE)malloc(Len);
						CopyMemory(pMem,szPacket,Len);

						Stage = 2;
					}
				}
				else if(*(PUINT)pPtr == -2) //Multipacket
				{
					INT HdrSize;

					IsMultipacket = TRUE;
					pPtr += 4;
					if(*(PUINT)pPtr & 0x80000000)
					{
						//Compressed??! FUCK YOU!
						return FALSE;
					}
					pPtr += 4;
					if(!Total) Total = *pPtr;
					pPtr++;
					Current = *pPtr++;

					if(!SwitchSize) //Avoid exploit
						SwitchSize = *(PSHORT)pPtr;
					pPtr += 2;

					HdrSize = (INT)(pPtr-szPacket);
					Len -= HdrSize;
					
					//Remove multipacket header
					MoveMemory(szPacket,szPacket+HdrSize,Len);

					if(Current == 0 || !pMem) //If first
					{
						//Allocate memory
						pMem = (PBYTE)calloc(Total,SwitchSize);
					}

					//Copy memory at given index (current)
					//Because UDP is unordered
					if(Current < Total)
						CopyMemory(pMem+SwitchSize*Current,szPacket,SwitchSize);
					if(Current + 1 == Total) //Last packet
						Stage = 2;
					else Stage = 1;
				}
			}
		}
		else return FALSE; //Time out!
	} while(Stage != 2);

	ParseRules(pMem,AddRule);
	free(pMem);
	return TRUE;
}

BOOL GetServerInfo(NETADR_T* Addr,AddServer_t AddServer,
	AddPlayer_t AddPlayer,AddRule_t AddRule)
{
	static BYTE szPacket[1240];
	CONNECTION_T m_conn;
	PBYTE pPtr;
	SERVER_T Server;
	UINT Challenge;

	m_conn.s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	m_conn.addr.sin_family = AF_INET;
	m_conn.addr.sin_addr = Addr->sin_addr;
	m_conn.addr.sin_port = Addr->sin_port;

	//MakeNonblocking(m_conn.s);

	//Request info

	sendto(m_conn.s,(const char*)szSourceQuery,sizeof(szSourceQuery),0,
		(const struct sockaddr*)&m_conn.addr,sizeof(struct sockaddr_in));
	if(RecvTimeOut(&m_conn,szPacket,sizeof(szPacket),3000))
	{
		ParsePacket(szPacket,&Server);
		AddServer(&Server);
	}
	else
	{
		closesocket(m_conn.s);
		return FALSE;
	}

	//Request players
	Challenge = (UINT)-1;

	pPtr = szPacket;
	*(PUINT)pPtr = (UINT)-1;
	pPtr += 4;
	*pPtr++ = 0x55;
	*(PUINT)pPtr = Challenge;
	pPtr += 4;

	//Got challenge
	sendto(m_conn.s,(const char*)szPacket,9,0,
		(const struct sockaddr*)&m_conn.addr,sizeof(struct sockaddr_in));
	if(RecvTimeOut(&m_conn,szPacket,sizeof(szPacket),3000))
	{
		Challenge = *(PUINT)(szPacket+5);
	}
	else
	{
		closesocket(m_conn.s);
		return FALSE;
	}

	pPtr = szPacket;
	*(PUINT)pPtr = (UINT)-1;
	pPtr += 4;
	*pPtr++ = 0x55;
	*(PUINT)pPtr = Challenge;
	pPtr += 4;

	sendto(m_conn.s,(const char*)szPacket,9,0,
		(const struct sockaddr*)&m_conn.addr,sizeof(struct sockaddr_in));
	if(RecvTimeOut(&m_conn,szPacket,sizeof(szPacket),3000))
	{
		//Parse players
		BYTE PlayerNum;
		PCHAR Name;
		UINT Score;
		FLOAT Duration;
		INT i;

		pPtr = szPacket;
		pPtr += 5;
		PlayerNum = *pPtr++;

		for(i = 0; i < PlayerNum; i++)
		{
			pPtr++;
			Name = (PCHAR)pPtr;
			pPtr += strlen((const char*)pPtr)+1;

			Score = *(PUINT)pPtr;
			pPtr += 4;

			Duration = *(PFLOAT)pPtr;
			pPtr += 4;
			
			AddPlayer(Name,Score,Duration);
		}
	}

	MakeNonblocking(m_conn.s);

	//Parse rules
	GetServerRules(&m_conn,AddRule);
	closesocket(m_conn.s);

	return TRUE;
}