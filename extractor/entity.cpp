#include "stdafx.h"
#include "extractorDlg.h"
#include "entity.h"
#include<process.h>
#include<Ws2tcpip.h>

//////////////////////////Class: Util//////////////////////////////////

int Util::g_max_running_task_num = 3;
int Util::g_max_sgmt_num = 5;

void Util::print_error(DWORD err, LPTSTR prompt)
{
    LPVOID lpMsgBuf;
    ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM, 0, err, 
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
    CString status;
    if (prompt)
        status.Format(TEXT("%s, %s"), prompt, (LPTSTR)lpMsgBuf);
    else
        status = (LPTSTR)lpMsgBuf;
    print_status(status);
//    MessageBox(NULL, (LPTSTR)lpMsgBuf, prompt, MB_OK|MB_ICONEXCLAMATION);
    LocalFree(lpMsgBuf);
}

void Util::print_status(CString status)
{
    ((CextractorDlg*)AfxGetApp()->GetMainWnd())->print_status(status);
}

LPTSTR Util::sdup(LPTSTR src)
{
    return sdup(src, _tcslen(src));
}

LPTSTR Util::sdup(LPTSTR src, size_t len)
{
    LPTSTR dst = new TCHAR[len + 1];
    _tcsncpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

LPTSTR Util::joint(LPTSTR s1, LPTSTR s2)
{
    if (!s1 || !s2)
        return NULL;
    LPTSTR dst = new TCHAR[_tcslen(s1) + _tcslen(s2) + 1];
    _tcscpy(dst, s1);
    _tcscat(dst, s2);
    return dst;
}

char* Util::wide_to_utf8(LPTSTR src)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
    if (len <= 0)
    {
        Util::print_error(GetLastError(), TEXT("WideCharToMultiByte required buffer size error"));
        return NULL;
    }
    char* dst = new char[len + 1];
    len = WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, len, NULL, NULL);
    if (len <= 0)
    {
        Util::print_error(GetLastError(), TEXT("WideCharToMultiByte error"));
        return NULL;
    }
    return dst;
}

bool Util::get_str_after_str(char *dst, int dst_len, const char **end_ch_ptr, const char *src, const char *substr, const char end_ch)
{
    if (!src || !*src || !substr || !*substr)
        return false;

    const char *p, *q;
    p = strstr(src, substr);
    if (p)
    {
        p += strlen(substr);
        for (q = p; *q != end_ch && *q != '\0'; q ++)
            ;
        if (q > p)
        {
            if ((q - p) < dst_len)
            {
                strncpy(dst, p, (q - p));
                dst[q - p] = '\0';
                if (end_ch_ptr)
                    *end_ch_ptr = q;
                return true;
            }
        }
    }
    return false;
}

//////////////////////////Class: Url//////////////////////////////////

Url::Url(LPTSTR host, LPTSTR file, LPTSTR port, int depth)
{
    this->m_host = host;
    this->m_file = file;
    this->m_port = port;
    this->m_depth = depth;
}

Url::~Url()
{
    if (this->m_host)
    {
        delete[] this->m_host;
        this->m_host = NULL;
    }
    if (this->m_file)
    {
        delete[] this->m_file;
        this->m_file = NULL;
    }
    if (this->m_port)
    {
        delete[] this->m_port;
        this->m_port = NULL;
    }
}

Url* Url::gen_url(LPTSTR str, int depth)
{
    // Judge whether str is a valid url.
    LPTSTR p, q;
    if (p = _tcsstr(str, TEXT("://")))
    {
        // ONLY support http protocal so far.
        if (_tcsncmp(str, TEXT("http"), 4) != 0)
            return NULL;
        p += 3;
    }
    else
        p = str;
    // beginning of hostname
    if (!*p)
        return NULL;
    q = p;
    while(*q && *q != TEXT('/') && *q != TEXT(':')) 
    {
        // To Lower
        *q = _totlower(*q);
        q ++;
    }
    if (q == p)
        return NULL;
    LPTSTR host = Util::sdup(p, q-p);
/*    
    USHORT port = 80;
    if (*q == TEXT(':'))
    {
        // Port num
        port = 0;
        while(*(++q) && _istdigit(*q))
        {
            port = port * 10 + *q - '0';
        }
    }
*/
    LPTSTR port;
    if (*q == TEXT(':'))
    {
        p = ++q;
        while(*q && _istdigit(*q))
            q++;
        port = Util::sdup(p, q-p);
    }
    else
    {
        port = Util::sdup(TEXT("80"));
    }

    if (*q != TEXT('/'))
    {
        // file name default to be "/".
        return new Url(host, Util::sdup(TEXT("/")), port, depth);
    }
    else
    {
        p = q;
        // TODO need to consider [;parameters][?query] and . / .. case.
        while(*(++q) && *q != TEXT('#') && *q != TEXT(';') && *q != TEXT('?'))
        {
            *q = _totlower(*q);
        }
        return new Url(host, Util::sdup(p, q-p), port, depth);
    }
}

//////////////////////////Class: Task//////////////////////////////////

Task::Task(Url *url, LPTSTR folder)
{
    this->m_status = ETR_STATUS_STOPPED;
    this->m_url = url;
    if (_tcscmp(url->m_file, TEXT("/")) == 0)
        this->m_base_name = Util::sdup(url->m_host);
    else
    {
        LPTSTR p = _tcsrchr(url->m_file, TEXT('/'));
        this->m_base_name = Util::sdup(++p);
    }
    this->m_local_path = Util::joint(folder, this->m_base_name);
    this->m_length = 0;
    this->m_sgmts = new Segment[Util::g_max_sgmt_num];
    for (int i = 0; i < Util::g_max_sgmt_num; i ++)
        this->m_sgmts[i].m_task = this;
}

Task::~Task()
{
    if (this->m_url)
    {
        delete this->m_url;
        this->m_url = NULL;
    }
    if (this->m_local_path)
    {
        delete[] this->m_local_path;
        this->m_local_path = NULL;
    }
    if (this->m_base_name)
    {
        delete[] this->m_base_name;
        this->m_base_name = NULL;
    }
    //TODO destroy m_sgmts.
}

void Task::set_status(ETR_STATUS status)
{
    this->m_status = status;
}

ETR_STATUS Task::get_status()
{
    return this->m_status;
}

void Task::process()
{
    if (this->m_length == 0)
    {
        _beginthreadex(NULL, 0, start_sgmt_wrapper, (void*)&this->m_sgmts[0], 0, NULL);
    }
    else
    {
        for (int i = 0; i < Util::g_max_sgmt_num; i ++)
            _beginthreadex(NULL, 0, start_sgmt_wrapper, (void*)&this->m_sgmts[i], 0, NULL);
    }
}

void Task::set_length(int length)
{
    this->m_length = length;
    int sgmt_length = length / Util::g_max_sgmt_num;
    for (int i = 0; i < Util::g_max_sgmt_num; i ++)
    {
        this->m_sgmts[i].m_start_pos = i * sgmt_length;
        if (i != Util::g_max_sgmt_num - 1)
            this->m_sgmts[i].m_end_pos = (i + 1) * sgmt_length - 1;
        else
            this->m_sgmts[i].m_end_pos = length -1;
        if (i != 0)
            _beginthreadex(NULL, 0, start_sgmt_wrapper, (void*)&this->m_sgmts[i], 0, NULL);
    }
}

//////////////////////////Class: Cacher///////////////////////////////////
Cacher::Cacher():m_cur_pos(0)
{
    this->m_cache_buff = new char[CACHE_BUFF_LENGTH];
    init_buff();
}

Cacher::~Cacher()
{
    if (this->m_cache_buff)
    {
        delete[] this->m_cache_buff;
        this->m_cache_buff = NULL;
    }
}

void Cacher::init_buff()
{
    ZeroMemory(this->m_cache_buff,CACHE_BUFF_LENGTH);
    this->m_cache_length = 0;
}

bool Cacher::write_file()
{
    if (this->m_cache_length <= 0)
        return true;
    // TODO handle write file error, stop and wait?
    CString file_name;
    file_name.Format(TEXT("%s.ext"), this->m_sgmt->m_task->m_local_path);
    HANDLE hd = CreateFile(file_name,                // name of the write
                       GENERIC_WRITE,          // open for writing
                       FILE_SHARE_READ|FILE_SHARE_WRITE, // full share
                       NULL,                   // default security
                       OPEN_ALWAYS,             // open always
                       FILE_ATTRIBUTE_NORMAL,  // normal file
                       NULL);                  // no attr. template
    if (hd == INVALID_HANDLE_VALUE)
    {
        Util::print_error(GetLastError(), TEXT("CreateFile fail"));
        return false;
    }
    if (SetFilePointer(hd, this->m_sgmt->m_start_pos, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        Util::print_error(GetLastError(), TEXT("SetFilePointer fail"));
        return false;
    }
    DWORD result;
    if (WriteFile(hd, this->m_cache_buff, this->m_cache_length, &result, NULL) == FALSE)
    {
        Util::print_error(GetLastError(), TEXT("WriteFile fail"));
        return false;
    }
    CloseHandle(hd);
    this->m_sgmt->m_start_pos += this->m_cache_length;
    init_buff();
    return true;
}

RangeCacher::RangeCacher(long pos)
{
    this->m_cur_pos = pos;
}

void RangeCacher::process(char *str, long length)
{
    if (this->m_sgmt->m_end_pos > 0)
    {
        long left = this->m_sgmt->m_end_pos - this->m_cur_pos + 1;
        if (length >= left)
            length = left;
    }
    if (this->m_cache_length + length > CACHE_BUFF_LENGTH)
    {
        write_file();
    }
    CopyMemory(this->m_cache_buff + this->m_cache_length, str, length);
    this->m_cache_length += length;
    this->m_cur_pos += length;
}

NoRangeCacher::NoRangeCacher():RangeCacher(0)
{
}

void NoRangeCacher::process(char *str, long length)
{
    // Skip the bytes already process.
    if (this->m_cur_pos + length <= this->m_sgmt->m_start_pos)
    {
        this->m_cur_pos += length;
        return ;
    }
    if (this->m_cur_pos < this->m_sgmt->m_start_pos)
    {
        long skip = this->m_sgmt->m_start_pos - this->m_cur_pos;
        length -= skip;
        str += skip;
    }
    RangeCacher::process(str, length);
}

ChunkCacher::ChunkCacher():m_chunk_size(-2), m_proc_bytes(0), m_meet_return(false)
{
}

void ChunkCacher::process(char *str, long length)
{
    while (length)
    {
        long return_pos = this->m_chunk_size + 2;
        // Skip ending \r\n
        while (this->m_proc_bytes >= this->m_chunk_size && this->m_proc_bytes < this->m_chunk_size + 2)
        {
            this->m_proc_bytes ++;
            str ++;
            length --;
            if (length == 0)
            {
                if (this->m_chunk_size == 0)
                {
                    // END!
                    Util::print_status(TEXT("END!!!!"));
                }
                return ;
            }
        }
        // Assert length > 0.
        if (this->m_proc_bytes == return_pos)
        {
            this->m_chunk_size = 0;
            this->m_proc_bytes = 0;
            this->m_meet_return = false;
        }
        if (!this->m_meet_return)
        {
            get_chunk_size(str, length);
        }
        else
        {
            // process left bytes.
            long left = this->m_chunk_size - this->m_proc_bytes;
            long min = (left < length) ? left : length;
            NoRangeCacher::process(str, min);
            str += min;
            this->m_proc_bytes += min;
            length -= min;
        }
    }
}

void ChunkCacher::get_chunk_size(char*& str, long& length)
{
    while(length)
    {
        length --;
        char ch = *str;
        if (isupper(ch))
            ch = tolower(ch);
        str ++;
        if (isdigit(ch))
        {
            this->m_chunk_size *= 16;
            this->m_chunk_size += ch - '0';
        }
        else if (islower(ch))
        {
            this->m_chunk_size *= 16;
            this->m_chunk_size += ch - 'a' + 10;
        }
        else if (ch == '\r')
        {
            // NOP
        }
        else if (ch == '\n')
        {
            CString status;
            status.Format(TEXT("Chunked: %d."), this->m_chunk_size);
            Util::print_status(status);
            this->m_meet_return = true;
            break;
        }
        else
        {
            // UN-EXPECTED character!
        }
    }
}

//////////////////////////Class: Segment//////////////////////////////////

Segment::Segment():m_start_pos(0), m_end_pos(0), m_cacher(NULL)
{
    this->m_recv_buff = new char[CACHE_BUFF_LENGTH];
}

Segment::~Segment()
{
    if (this->m_recv_buff)
    {
        delete[] this->m_recv_buff;
        this->m_recv_buff = NULL;
    }
}

unsigned __stdcall start_sgmt_wrapper(void* pArguments)
{
    ((Segment*)pArguments)->start_sgmt();
    return 0;
}

/* Http request: GET /filename HTTP/1.1\r\nHost:hostname\r\nRange: bytes=0-1024\r\n[Accept-Encoding: gzip,deflate,sdch]\r\n\r\n
Host must be given or will get a "bad request" response,
and the last \r\n\r\n are also required!
*/
void Segment::start_sgmt()
{
    while((this->m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
    {
        Util::print_error(WSAGetLastError());
        Sleep(1000);
    }

    ADDRINFOT *result = NULL;
    int rc;
    while ((rc = GetAddrInfo(this->m_task->m_url->m_host, this->m_task->m_url->m_port, NULL, &result)) != NO_ERROR)
    {
        Util::print_error(rc, this->m_task->m_url->m_host);
        Sleep(5000);
    }

RECONNECT:    while ((rc = connect(this->m_socket, result->ai_addr, sizeof(sockaddr))) != NO_ERROR)
    {
        Util::print_error(WSAGetLastError(), TEXT("connect error"));
        Sleep(5000);
    }

    // Send request
    char buff[1024];
    char* host = Util::wide_to_utf8(this->m_task->m_url->m_host);
    char* file = Util::wide_to_utf8(this->m_task->m_url->m_file);
    sprintf(buff, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/34.0.1847.137 Safari/537.36\r\n", file, host);
    // TODO add encoding support
//    strcat(buff, "Accept-Encoding: gzip,deflate,sdch\r\n");
    delete[] host;
    delete[] file;
    if (this->m_end_pos != 0)
    {
        sprintf(buff+strlen(buff), "Range: bytes=%d-%d\r\n", this->m_start_pos, this->m_end_pos);
    }
    sprintf(buff+strlen(buff), "\r\n\r\n");
    if ((rc = send(this->m_socket, buff, strlen(buff), 0)) == SOCKET_ERROR)
    {
        Util::print_error(WSAGetLastError(), TEXT("send error"));
        goto RECONNECT;
    }
/*
    // If iMode != 0, non-blocking mode is enabled.
    u_long mode = 1;
    rc = ioctlsocket(this->m_socket, FIONBIO, &mode);
    if (rc != NO_ERROR)
    {
        Util::print_error(rc);
    }
*/    
    // Add socket to Monitor
    Monitor::singleton()->add_sgmt(this);
/*
    if ((rc = recv(this->m_socket, this->m_recv_buff, RECV_BUFF_LENGTH, 0)) == SOCKET_ERROR)
    {
        // Error code would be WSAEWOULDBLOCK.
        Util::print_error(WSAGetLastError(), TEXT("recv error"));
    }
*/
    this->start_recv();
    Util::print_status(TEXT("start_sgmt"));
}

void Segment::start_recv(int skip_bytes)
{
    WSABUF data_buf;
    char *p = this->m_recv_buff;
    if (skip_bytes == 0)
        data_buf.len = RECV_BUFF_LENGTH;
    else
    {
        data_buf.len = RECV_BUFF_LENGTH - skip_bytes;
        p += skip_bytes;
    }
    data_buf.buf = p;
    ZeroMemory(p,data_buf.len);
    DWORD recv_bytes;
    DWORD flags = 0;
    // Have to specify overlapped, otherwise, completion port could not be notified.
    WSAOVERLAPPED overlapped;
    ZeroMemory(&overlapped,sizeof(WSAOVERLAPPED));
    int rc;
    if ((rc = WSARecv(this->m_socket, &data_buf, 1, &recv_bytes, &flags, &overlapped, NULL)) == SOCKET_ERROR)
    {
        rc = WSAGetLastError();
        if (rc != WSA_IO_PENDING)
            Util::print_error(WSAGetLastError(), TEXT("recv error"));
    }
}

/* HTTP reponse header:
HTTP/1.1 200 OK
Age: 93083
Via: 1.1 57dc94f14e76f76880bbcef7cac59cff.cloudfront.net (CloudFront)
Date: Tue, 20 May 2014 07:37:10 GMT
ETag: "2106816252"
Server: nginx
X-Whom: l3-com-cyber
Expires: Thu, 27 Jan 2028 07:37:10 GMT
X-Cache: Hit from cloudfront
X-Amz-Cf-Id: Cc2eQk-Emj1thw_OPmoeOwFsS3drA6SCXsGr2ohKl0RZCoyF0jHnZA==
Content-Type: image/png
Accept-Ranges: bytes
Cache-Control: public, max-age=432000000
Last-Modified: Mon, 05 Nov 2012 23:06:34 GMT
X-Origin-Type: StaticViaCBZORG
Content-Length: 30354
Proxy-Connection: Keep-Alive

HTTP/1.1 304 Not Modified
Via: 1.1 9a21a56736b9b717cebcaf88eb2e42f2.cloudfront.net (CloudFront)
Date: Wed, 21 May 2014 09:23:26 GMT
ETag: "2106816252"
Server: nginx
X-Whom: l2-com-cyber
Expires: Fri, 28 Jan 2028 09:23:26 GMT
X-Cache: Miss from cloudfront
X-Amz-Cf-Id: L6MnFffEpxnJhDhgAhMuonO8J7UjVGlzVpc2LUD2o9nA5iufO5EZgw==
Content-Type: image/png
Accept-Ranges: bytes
Cache-Control: public, max-age=432000000
Last-Modified: Mon, 05 Nov 2012 23:06:34 GMT
X-Origin-Type: StaticViaCBZORG
Proxy-Connection: Keep-Alive
*/
void Segment::recv_ready(DWORD recv_bytes)
{
    long len = recv_bytes;
    char *p = this->m_recv_buff;
    // Cacher would determine the process procedure.
    if (this->m_cacher == NULL)
    {
        // Make sure header is received.
        p = strstr(this->m_recv_buff, "\r\n\r\n");
        if (p == NULL)
        {
            this->start_recv(recv_bytes);
            return ;
        }
        // Parse HTTP header.
        ETR_RESPONSE res = this->parse_header();
        if (res == ETR_RESPONSE_ERROR)
        {
            // TODO reconnect.
            return;
        }
        p += 4;
        len -= (p - this->m_recv_buff);
        switch(res)
        {
        case ETR_RESPONSE_CHUNKED:
            m_cacher = new ChunkCacher();
            break;
        case ETR_RESPONSE_NO_RANGES:
            m_cacher = new NoRangeCacher();
            break;
        case ETR_RESPONSE_ACCEPT_RANGES:
            m_cacher = new RangeCacher(this->m_start_pos);
            break;
        }
        m_cacher->m_sgmt = this;
    }
    m_cacher->process(p, len);
    this->start_recv();
}

ETR_RESPONSE Segment::parse_header()
{
    const int STR_LEN = 256;
    char str[STR_LEN];
    // Get status code
    // HTTP/1.1 200 OK
    if (!Util::get_str_after_str(str, STR_LEN, NULL, this->m_recv_buff, " ") || str[0] != '2')
        return ETR_RESPONSE_ERROR;
    // Transfer-Encoding: chunked
    if (Util::get_str_after_str(str, STR_LEN, NULL, this->m_recv_buff, "Transfer-Encoding: ", '\r') && strcmp(str, "chunked") == 0)
    {
        return ETR_RESPONSE_CHUNKED;
    }
    if (!Util::get_str_after_str(str, STR_LEN, NULL, this->m_recv_buff, "Accept-Ranges: ", '\r') || strcmp(str, "bytes") != 0)
    {
        CString status;
        status.Format(TEXT("%s does not support range transfer."), this->m_task->m_url->m_host);
        Util::print_status(status);
        return ETR_RESPONSE_NO_RANGES;
    }
    if (Util::get_str_after_str(str, STR_LEN, NULL, this->m_recv_buff, "Content-Length: ", '\r'))
    {
        int length = atoi(str);
        if (length > 0)
        {
            if (this->m_task->m_length == 0)
            {
                this->m_task->set_length(length);
            }
            else if (this->m_task->m_length != length)
            {
                // TODO File is updated, need to re-download.
            }
        }
    }
    return ETR_RESPONSE_ACCEPT_RANGES;
}

void Segment::recv_closed()
{
    if (this->m_cacher)
    {
        this->m_cacher->write_file();
    }
    // TODO restart segment if not complete?
    if (this->m_start_pos < this->m_end_pos)
    {
        Util::print_status(TEXT("Segment not completed, restart!"));
        this->start_sgmt();
        return;
    }
    // TODO complete.
}

void Segment::recv_stoped()
{
}

//////////////////////////Class: Monitor//////////////////////////////////

Monitor::Monitor(void)
{
    if ((this->m_hiocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0)) == NULL)
    {
        DWORD err = GetLastError();
        Util::print_error(err, TEXT("CreateIoCompletionPort fail"));
        exit(err);
    }
}

Monitor::~Monitor(void)
{
    if (this->m_hiocp)
    {
        CloseHandle(this->m_hiocp);
        this->m_hiocp = NULL;
    }
}

Monitor* Monitor::singleton()
{
    static Monitor _instance;
    return &_instance;
}

void Monitor::add_sgmt(Segment* sgmt)
{
    if ((this->m_hiocp = CreateIoCompletionPort((HANDLE)sgmt->m_socket, this->m_hiocp, (ULONG_PTR)sgmt, 0)) == NULL)
    {
        DWORD err = GetLastError();
        Util::print_error(err, TEXT("CreateIoCompletionPort for sgmt fail"));
        exit(err);
    }
}

void Monitor::monitor()
{
    DWORD recv_bytes;
    Segment* sgmt;
    LPOVERLAPPED overlapped = NULL;
    while(GetQueuedCompletionStatus(this->m_hiocp, &recv_bytes, (PULONG_PTR)&sgmt, &overlapped, INFINITE))
    {
        CString status;
        status.Format(TEXT("Recv: %d."), recv_bytes);
        Util::print_status(status);
        if (recv_bytes == 0)
        {
            if (sgmt == NULL)
            {
                // TODO being stopped.
                Util::print_status(TEXT("STOPPED"));
            }
            else
            {
                // TODO sgmt socket is closed.
                Util::print_status(TEXT("ClOSED"));
                sgmt->recv_closed();
            }
        }
        else
        {
            // TODO sgmt data is ready.
            Util::print_status(TEXT("DATA READY"));
            sgmt->recv_ready(recv_bytes);
        }
    }
    DWORD err = GetLastError();
    Util::print_error(err, TEXT("GetQueuedCompletionStatus fail"));
    exit(err);
}