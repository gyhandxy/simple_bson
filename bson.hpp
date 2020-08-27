#ifndef __BSON_HPP__
#define __BSON_HPP__
#include<map>
#include<string>
#include<vector>
#include<string>
//#include"../common.hpp"

namespace bson{
    //仅支持所需要的ascii string、document、array、字节数组、bool、32位有符号数、64位有符号数
    static bool supportArray[]={
        false,//double
        true,//utf8
        true,//embedded document
        true,//array
        true,//binary data
        false,//undefined
        false,//objectId
        true,//boolean false/true
        false,//utc datetime
        false,//NULL
        false,//cstring
        false,//dbpointer
        false,//javascript code
        false,//symbol
        false,//js code w
        true,//32bit integer
        false,//unsigned int64 timestamp
        true,//64bit signed integer
        false,//128bit decimal floating point
    };
    //supportArray大小
    static const int N=sizeof(supportArray)/sizeof(bool);
    //为支持的类型进行枚举,NO_SET用于和判定生成无效element
    enum supportedElementType {STRING,EMBEDDED_DOCUMENT,ARRAY_DOCUMENT,BYTE_ARRAY,BOOL,INT32,INT64,NO_SET};

    class document;
    class element{
    private:
        std::string elementName;
        supportedElementType elementType;
        //element依附于document，ptr将指向document的数据
        const unsigned char *ptr;
    public:
        std::string getelementName();
        bson::supportedElementType getelementType();
        
        bool emptyElement();
        element();
        element(std::string element_name,supportedElementType element_type,const unsigned char *pointer);
        
        //string
        bool isString();
        bool emptyString();
        std::string String();
        //document
        bool isDocument();
        document Document();
        //array
        bool isArray();
        document Array();
        //binary
        bool isBinary();
        //使用Binary之后需要delete[]接受的指针
        unsigned char* Binary(int& n);
        //boolean
        bool isBool();
        bool Bool();
        //int
        bool isInt();
        int32_t Int32();
        int64_t Int64();
        //同Int32
        int Int();
        //将byte数组转换为json string
        std::string byteArray2JsonString();
        //为document中的toJsonString所使用，这里的toJsonString仅返回表示字段值的string
        std::string toJsonString();
    };

    class document{
    private:
        bool valid(const unsigned char *msgdata);
        int lenOfrawdata;//保存rawdataArr中数据的长度
        bool imArray;
        std::vector<unsigned char> rawdataArr;
        std::map<std::string,int> fields;
        std::map<std::string,supportedElementType> typeMap;
    public:
        bool judgeParse;
        unsigned char* rawdata;

        document();//默认情况为5,0,0,0,0
        document(const unsigned char *msgdata,int n);//将会保存数据
        document(const document& doc);
        document(document&& doc);

        void defaultConstruct();//初始化七个变量
        int size() const;
        bool emptyArray();
        bool isArray();
        void setIsArray(bool boolean);
        bool parseWithSaveData(const unsigned char *msgdata,int n);
        bool hasField(std::string field_name) const;
        element operator[](std::string field_name) const;
        int max_n();//返回数组的长度
        bool isDocument() const;
        std::string toJsonString();
        void append(std::string e_name,std::string value);
        void append(std::string e_name,const char *value);
        void append(std::string e_name,document& doc);
        void append(std::string e_name,unsigned char *binary,int n);
        void append(std::string e_name,bool boolean);
        void appendInt32(std::string e_name,int32_t value);
        void appendInt64(std::string e_name,int64_t value);
        void update();//可连续多次调用；在下次使用lenOfrawdata（一般是append里面的最后）之前必须调用
        void addData(const unsigned char *to_append,int n);//添加大量数据时使用；其他情况请对rawdataArr进行push_back之后调用update来更新lenOfrawdata和rawdata
    };
}
#endif
