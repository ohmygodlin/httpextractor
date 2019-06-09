#pragma once

#include<list>
using namespace std;

enum ETR_STATUS
{
    ETR_STATUS_STOPPED,
    ETR_STATUS_PENDING,
    ETR_STATUS_RUNNING,
    ETR_STATUS_COMPLETED
};

const CString ETR_STATUS_STR[] = {
    TEXT("STOPPED"),
    TEXT("PENDING"),
    TEXT("RUNNING"),
    TEXT("COMPLETED"),
};

class Util
{
public:
    static void print_error(DWORD err, LPTSTR prompt = NULL);
    static void print_status(CString status);
    static LPTSTR sdup(LPTSTR src);
    static LPTSTR sdup(LPTSTR src, size_t len);
    static LPTSTR joint(LPTSTR s1, LPTSTR s2);
    static char* wide_to_utf8(LPTSTR src);
    static bool get_str_after_str(char* dst, int dst_len, const char** end_ch_ptr, const char* src, const char* substr, const char end_ch = ' ');

    static int g_max_running_task_num;
    static int g_max_sgmt_num;
};

const int DEFAULT_DEPTH = 2;
const int RECV_BUFF_LENGTH = 8*1024;
const int CACHE_BUFF_LENGTH = 1*1024*1024;

class Url
{
public:
    Url(LPTSTR host, LPTSTR file, LPTSTR port, int depth);
    ~Url();
    static Url* gen_url(LPTSTR str, int depth = DEFAULT_DEPTH);
// Ignore setter and getter.
    LPTSTR m_host;
    LPTSTR m_file;
    LPTSTR m_port;
    int m_depth;
};

class Segment;

class Task
{
public:
    Task(Url* url, LPTSTR folder);
    ~Task();
    void process();
    void set_status(ETR_STATUS status);
    ETR_STATUS get_status();
    // Set length and start new sgmts.
    void set_length(int length);
// Ignore setter and getter.
    Url* m_url;
    LPTSTR m_local_path;
    LPTSTR m_base_name;
    ETR_STATUS m_status;
    int m_length;
    Segment* m_sgmts;
};

class Cacher
{
public:
    Cacher();
    ~Cacher();
    void init_buff();
    bool write_file();
    virtual void process(char* str, long length) = 0;
    char* m_cache_buff;
    long m_cache_length;
    long m_cur_pos;
    Segment* m_sgmt;
};

class RangeCacher : public Cacher
{
public:
    RangeCacher(long pos);
    ~RangeCacher();
    virtual void process(char* str, long length);
};

class NoRangeCacher : public RangeCacher
{
public:
    NoRangeCacher();
    ~NoRangeCacher();
    virtual void process(char* str, long length);
};

class ChunkCacher : public NoRangeCacher
{
public:
    ChunkCacher();
    ~ChunkCacher();
    virtual void process(char* str, long length);
    void get_chunk_size(char*& str, long& length);
    long m_chunk_size;
    long m_proc_bytes;
    bool m_meet_return;
};

enum ETR_RESPONSE
{
    ETR_RESPONSE_ERROR = -1,
    ETR_RESPONSE_CHUNKED = 0,
    ETR_RESPONSE_NO_RANGES,
    ETR_RESPONSE_ACCEPT_RANGES,
};

unsigned __stdcall start_sgmt_wrapper(void* pArguments);

class Segment
{
public:
    Segment();
    ~Segment();
    void start_sgmt();     // Thread function
    void start_recv(int skip_bytes = 0);
    void recv_ready(DWORD recv_bytes);
    void recv_stoped();
    void recv_closed();
    ETR_RESPONSE parse_header();
// Ignore setter and getter.
    SOCKET m_socket;
    long m_start_pos;
    long m_end_pos;
    char* m_recv_buff;
    Task* m_task;
    Cacher* m_cacher;
};

class Host
{
};

class Monitor
{
public:
    static Monitor* singleton();
    void monitor();
    void add_sgmt(Segment* sgmt);
    void stop();
protected:
    Monitor(void);
    ~Monitor(void);
private:
    HANDLE m_hiocp;
};
