#include "bson.hpp"
#include<iostream>
using namespace std;

#define DOCUMENT_DEFAULT_SIZE    5

#define UNSIGNED_CHAR_0 0x00
#define UNSIGNED_CHAR_1 0x01
#define UNSIGNED_CHAR_5 0x05

#define END_OF_ENAME        0x00
#define END_OF_DOCUMENT     0x00

#define END_OF_STRING       0x00//string末尾会有一个0标志结束
#define BYTE_ARRAY_TYPE     0x00//标识byte array的种类，恒为0

#define TYPE_STRING         0x02
#define TYPE_DOCUMENT       0x03
#define TYPE_ARRAY          0x04
#define TYPE_BYTE_ARRAY     0x05
#define TYPE_BOOL           0x08
#define TYPE_INT32          0x10
#define TYPE_INT64          0x12

#define START_OF_DATA_USE_FOR_ELEMENT ptr+elementName.size()+1//用于element中跳过e_name，到达数据

#define TO_INT32_USE_FOR_GENERAL(x) \
                                    (int32_t)x[0]| \
                                    (int32_t)x[1]<<8| \
                                    (int32_t)x[2]<<16| \
                                    (int32_t)x[3]<<24

#define TO_INT32_USE_FOR_VALID(x) \
                                    (int32_t)x[index]| \
                                    (int32_t)x[index+1]<<8| \
                                    (int32_t)x[index+2]<<16| \
                                    (int32_t)x[index+3]<<24

#define TO_INT64_USE_FOR_GENERAL(x) \
                                    (int64_t)x[0]| \
                                    (int64_t)x[1]<<8| \
                                    (int64_t)x[2]<<16| \
                                    (int64_t)x[3]<<24| \
                                    (int64_t)x[4]<<32| \
                                    (int64_t)x[5]<<40| \
                                    (int64_t)x[6]<<48| \
                                    (int64_t)x[7]<<56

#define AFTER_APPEND \
                        do{ \
                            memcpy(rawdata,&lenOfrawdata,4); \
                        }while(0)

#define PUSH_ENAME(x) \
                    do{ \
                        if(analyzed){ \
                            typeMap[e_name]=x; \
                            fields[e_name]=lenOfrawdata; \
                        } \
                        memcpy(rawdata+lenOfrawdata,&(e_name[0]),e_name.size()); \
                        lenOfrawdata+=e_name.size(); \
                        rawdata[lenOfrawdata]=END_OF_ENAME; \
                        lenOfrawdata++; \
                    }while(0)

std::string bson::element::getelementName(){
    return elementName;
}
bson::supportedElementType bson::element::getelementType(){
    return elementType;
}
bool bson::element::emptyElement(){
    if(elementType==bson::supportedElementType::NO_SET){
        return true;
    }
    return false;
}
bson::element::element():elementName("no-set"),elementType(bson::supportedElementType::NO_SET),ptr(NULL){}
bson::element::element(std::string element_name,supportedElementType element_type,const unsigned char *pointer):elementName(element_name),elementType(element_type),ptr(pointer){}
bool bson::element::isString(){
    if(elementType==bson::supportedElementType::STRING){
        return true;
    }
    return false;
}
bool bson::element::emptyString(){
    //由于String()==""有两种情况，所以需要多判定一下类型
    if(elementType==bson::supportedElementType::STRING&&String()==""){
        return true;
    }
    return false;
}
std::string bson::element::String(){
    if(!isString()){
        return "";
    }
    const unsigned char *startOfString=START_OF_DATA_USE_FOR_ELEMENT;
    //跳过string数据首部的表示长度的4字节，因为string以0结尾，且document已分析过数据正确性
    int index=4;
    std::string res="";
    while(startOfString[index]!=UNSIGNED_CHAR_0){
        res+=static_cast<char>(startOfString[index]);
        index++;
    }
    return res;
}
bool bson::element::isDocument(){
    if(elementType==bson::supportedElementType::EMBEDDED_DOCUMENT){
        return true;
    }
    return false;
}
bson::document bson::element::Document(){
    bson::document res;
    //返回一个无用对象，其judgeParse为false
    if(!isDocument()){
        res.judgeParse=false;
        return res;
    }
    const unsigned char *startOfDocument=START_OF_DATA_USE_FOR_ELEMENT;
    
    int32_t lenOfDocument=TO_INT32_USE_FOR_GENERAL(startOfDocument);
    res.saveDataAndParse(startOfDocument, lenOfDocument,true);
    return res;
}
bool bson::element::isArray(){
    if(elementType==bson::supportedElementType::ARRAY_DOCUMENT){
        return true;
    }
    return false;
}
bson::document bson::element::Array(){
    bson::document res;
    if(!isArray()){
        res.judgeParse=false;
        return res;
    }
    const unsigned char *startOfArray=START_OF_DATA_USE_FOR_ELEMENT;
    uint32_t lenOfBinary=TO_INT32_USE_FOR_GENERAL(startOfArray);
    if(res.saveDataAndParse(startOfArray, lenOfBinary,true)){
        res.setIsArray(true);
    }
    return res;
}
bool bson::element::isBinary(){
    if(elementType==bson::supportedElementType::BYTE_ARRAY){
        return true;
    }
    return false;
}
unsigned char* bson::element::Binary(int& n){
    if(!isBinary()){
        return NULL;
    }
    const unsigned char *startOfBinary=START_OF_DATA_USE_FOR_ELEMENT;
    int32_t lenOfBinary=TO_INT32_USE_FOR_GENERAL(startOfBinary);
    n=static_cast<int>(lenOfBinary);
    //跳过首部表示长度的4个字节、表示类型的一个字节（为0）
    int index=5;
    unsigned char *ret=new unsigned char[lenOfBinary+1];

    while(index-5<n){
        ret[index-5]=startOfBinary[index];
        index++;
    }
    //末尾添加0，因cmdhost.cpp中的sendRawPacketStream中需要将该数组转换为string来发送
    ret[lenOfBinary]=UNSIGNED_CHAR_0;
    return ret;
}
bool bson::element::isBool(){
    if(elementType==bson::supportedElementType::BOOL){
        return true;
    }
    return false;
}
bool bson::element::Bool(){
    if(!isBool()){
        return false;
    }
    const unsigned char *startOfBool=START_OF_DATA_USE_FOR_ELEMENT;
    if(startOfBool[0]==UNSIGNED_CHAR_0){
        return false;
    }
    if(startOfBool[0]==UNSIGNED_CHAR_1){
        return true;
    }
    return false;
}
bool bson::element::isInt(){
    if(elementType==bson::supportedElementType::INT32||elementType==bson::supportedElementType::INT64){
        return true;
    }
    return false;
}
int32_t bson::element::Int32(){
    if(!isInt()){
        return 0;
    }
    const unsigned char *startOfInt32=START_OF_DATA_USE_FOR_ELEMENT;
    int32_t ret=TO_INT32_USE_FOR_GENERAL(startOfInt32);
    return ret;
}
int bson::element::Int(){
    if(!isInt()){
        return 0;
    }
    const unsigned char *startOfInt32=START_OF_DATA_USE_FOR_ELEMENT;
    int32_t ret=TO_INT32_USE_FOR_GENERAL(startOfInt32);
    return static_cast<int>(ret);
}
int64_t bson::element::Int64(){
    if(!isInt()){
        return 0;
    }
    const unsigned char *startOfInt64=START_OF_DATA_USE_FOR_ELEMENT;
    int64_t ret=TO_INT64_USE_FOR_GENERAL(startOfInt64);
    return ret;
}
std::string bson::element::byteArray2JsonString(){
    if(!isBinary()){
        return "";
    }
    int n;
    unsigned char *binary_data=Binary(n);
    std::string res="[";
    for(int i=0;i<n;i++){
        if(i>0){
            res+=",";
        }
        res+=std::to_string(static_cast<int>(binary_data[i]));
    }
    res+="]";
    delete[] binary_data;
    return res;
}
std::string bson::element::toJsonString(){
    std::string res="";
    if(elementType==bson::supportedElementType::STRING){
        res+="\"";
        res+=String();
        res+="\"";
        return res;
    }
    if(elementType==bson::supportedElementType::EMBEDDED_DOCUMENT){
        return Document().toJsonString();
    }
    if(elementType==bson::supportedElementType::ARRAY_DOCUMENT){
        bson::document arr=Array();
        return arr.toJsonString();
    }
    if(elementType==bson::supportedElementType::BYTE_ARRAY){
        return byteArray2JsonString();
    }
    if(elementType==bson::supportedElementType::BOOL){
        if(Bool()){
            return "true";
        }else{
            return "false";
        }
    }
    if(elementType==bson::supportedElementType::INT32){
        return std::to_string(static_cast<int>(Int32()));
    }
    if(elementType==bson::supportedElementType::INT64){
       return std::to_string(static_cast<long long>(Int64()));
    }
    return res;
}


bool bson::document::valid(const unsigned char *msgdata){
    int32_t len=TO_INT32_USE_FOR_GENERAL(msgdata);
    //判断首部传入的长度n是否等于数据中自带的长度信息
    if(len!=static_cast<int32_t>(lenOfrawdata)){
        return false;
    }
    //判断末尾是否为0
    if(msgdata[len-1]!=UNSIGNED_CHAR_0){
        return false;
    }

    int32_t index=4;
    int label;
    while(index<len-1){
        //读取每个字段
        //e_name="";
        label=-1;
        label=static_cast<int>(msgdata[index]);
        //如果是不支持的格式
        if(label<0||label>N||supportArray[label-1]==false){
            return false;
        }
        index++;
        //强行转换
        std::string e_name((const char*)(msgdata+index));
        fields[e_name]=index;
        index+=(e_name.size()+1);
        //通过分析，跳跃至下一个字段token
        //在每个case中，length表示e_name之前标识的该字段总长度
        switch (label) {
            case 2://string=>int32 bytes 0x00
            {
                typeMap[e_name]=bson::supportedElementType::STRING;
                int32_t length=TO_INT32_USE_FOR_VALID(msgdata);
                if(msgdata[index+3+length]!=UNSIGNED_CHAR_0){
                    return false;
                }
                index+=(4+length);
                break;
            }
            case 3://embedded document=>int32 ... 0x00
            {
                typeMap[e_name]=bson::supportedElementType::EMBEDDED_DOCUMENT;
                int32_t length=TO_INT32_USE_FOR_VALID(msgdata);
                if(msgdata[index+length-1]!=UNSIGNED_CHAR_0){
                    return false;
                }
                index+=length;
                break;
            }
            case 4://array=>int32 ... 0x00
            {
                typeMap[e_name]=bson::supportedElementType::ARRAY_DOCUMENT;
                int32_t length=TO_INT32_USE_FOR_VALID(msgdata);
                if(msgdata[index+length-1]!=UNSIGNED_CHAR_0){
                    return false;
                }
                index+=length;
                break;
            }
            case 5://byte array=>int32 0x00 bytes
            {
                typeMap[e_name]=bson::supportedElementType::BYTE_ARRAY;
                int32_t length=TO_INT32_USE_FOR_VALID(msgdata);
                index+=(5+length);
                break;
            }
            case 8://boolean=>0x01/0x00
            {
                typeMap[e_name]=bson::supportedElementType::BOOL;
                if(msgdata[index]!=UNSIGNED_CHAR_0&&msgdata[index]!=UNSIGNED_CHAR_1){
                    return false;
                }
                index++;
                break;
            }
            case 16://int32=>0xYY 0xYY 0xYY 0xYY
            {
                typeMap[e_name]=bson::supportedElementType::INT32;
                index+=4;
                break;
            }
            case 18://int64=>0xYY 0xYY 0xYY 0xYY 0xYY 0xYY 0xYY 0xYY
            {
                typeMap[e_name]=bson::supportedElementType::INT64;
                index+=8;
                break;
            }
            default:
            {
                return false;
            }
        }
    }//end of while
    return true;
}//end of valid

//TODO，分配策略需要改变
void bson::document::grow(int require){
    if(lenOfrawdata+require>capacity){
        int newCapacity=2*capacity;
        if(newCapacity==0){
            newCapacity=64;
        }
        while(newCapacity<lenOfrawdata+require){
            newCapacity*=2;
        }
        rawdata=static_cast<unsigned char*>(realloc(rawdata, newCapacity+5));
        capacity=newCapacity;
    }
}
void bson::document::defaultConstruct(){
    fields.clear();
    typeMap.clear();
    setIsArray(false);
    capacity=0;
    lenOfrawdata=0;
    rawdata=nullptr;
    analyzed=false;
    
    //grow将会更新capacity和rawdata
    grow(DOCUMENT_DEFAULT_SIZE);
    unsigned char defaultData[DOCUMENT_DEFAULT_SIZE]={UNSIGNED_CHAR_5,UNSIGNED_CHAR_0,UNSIGNED_CHAR_0,UNSIGNED_CHAR_0,UNSIGNED_CHAR_0};
    memcpy(rawdata,defaultData,DOCUMENT_DEFAULT_SIZE);
    lenOfrawdata+=DOCUMENT_DEFAULT_SIZE;

    judgeParse=true;
}
//equal to lenOfrawdata
int bson::document::size() const{
    return lenOfrawdata;
}
bool bson::document::emptyArray(){
    if(imArray&&fields.empty()){
        return true;
    }
    return false;
}
bool bson::document::isArray(){
    if(imArray){
        return true;
    }
    return false;
}
void bson::document::setIsArray(bool boolean){
    imArray=boolean;
}
//默认情况为5,0,0,0,0
bson::document::document(){
    defaultConstruct();
}
bson::document::document(const unsigned char *msgdata,int n,bool needAnalysis){
    //needAnalysis将会控制是否进行parse
    saveDataAndParse(msgdata,n,needAnalysis);
}
//修改拷贝构造函数时，需要注意：grow会参考并更改capacity，不会影响lenOfrawdata
bson::document::document(const document& doc){
    fields.clear();
    typeMap.clear();
    imArray=doc.imArray;
    fields=doc.fields;
    typeMap=doc.typeMap;
    judgeParse=doc.judgeParse;
    rawdata=nullptr;
    analyzed=doc.analyzed;
    lenOfrawdata=0;
    capacity=0;
    grow(doc.capacity);
    memcpy(rawdata,doc.rawdata,doc.lenOfrawdata);
    lenOfrawdata=doc.lenOfrawdata;
}
bson::document::document(document&& doc){
    analyzed=std::move(doc.analyzed);
    imArray=std::move(doc.imArray);
    fields=std::move(doc.fields);
    typeMap=std::move(doc.typeMap);
    judgeParse=std::move(doc.judgeParse);
    lenOfrawdata=std::move(doc.lenOfrawdata);
    capacity=std::move(doc.capacity);
    rawdata=std::move(doc.rawdata);
    doc.rawdata=nullptr;
}
bool bson::document::saveDataAndParse(const unsigned char *msgdata,int n,bool needAnalysis){
    fields.clear();
    typeMap.clear();
    setIsArray(false);
    capacity=0;
    lenOfrawdata=0;
    rawdata=nullptr;
    grow(n);

    if(n<=0){
        judgeParse=false;
        return false;
    }
    memcpy(rawdata,msgdata,n);
    lenOfrawdata=n;
    
    if(needAnalysis&&valid(rawdata)){
        judgeParse=true;
    }else{
        judgeParse=false;
    }

    analyzed=needAnalysis;

    return judgeParse;
}
bool bson::document::hasField(std::string field_name) const{
    if(fields.find(field_name)==fields.end()||typeMap.find(field_name)==typeMap.end()){
        return false;
    }
    return true;
}
bool bson::document::getAnalyzed(){
    return analyzed;
}
void bson::document::analyze(){
    if(!analyzed){
        if(valid(rawdata)){
            judgeParse=true;
        }else{
            judgeParse=false;
        }
    }
}
bson::element bson::document::operator[](std::string field_name) const{
    auto get_element=fields.find(field_name);
    if(get_element==fields.end()){
        bson::element ret;
        return ret;
    }
    auto get_element_type=typeMap.find(field_name);
    int get_element_index=get_element->second;
    bson::element ret(field_name,get_element_type->second,(unsigned char*)(rawdata+get_element_index));
    return ret;
}
int bson::document::max_n() {
    if(!isArray()){
        return -1;
    }
    int n=0;
    int tmp=-1;
    for(auto &iter:fields){
        tmp=atoi((iter.first).c_str());
        n=n>tmp?n:tmp;
    }
    return n;
}
bool bson::document::isDocument() const{
    return judgeParse;
}
std::string bson::document::toJsonString() {
    analyze();
    if(!judgeParse){
        return "";
    }
    if(isArray()){
        std::string res="[";
        int getMaxN=max_n();
        for(int i=0;i<getMaxN;i++){
            if(i>0){
                res+=",";
            }
            bson::element elem=this->operator[](std::to_string(i));
            res+=elem.toJsonString();
        }
        res+="]";
        return res;
    }
    
    int counter=0;
    std::string res="{";
    for(auto &iter:fields){
        if(counter!=0){
            res+=",";
        }else{
            counter++;
        }
        res+="\"";
        res+=iter.first;
        res+="\":";
        bson::element tmp=this->operator[](iter.first);
        res+=tmp.toJsonString();
    }
    res+="}";
    return res;
}
void bson::document::append(std::string e_name,std::string value){
    grow(static_cast<int>(e_name.size()+value.size()+7));//-1+1+e_name.size()+1+4+value.size()+1+1
    
    //push type string
    rawdata[lenOfrawdata-1]=TYPE_STRING;
    
    PUSH_ENAME(bson::supportedElementType::STRING);

    //push value:length of value
    int32_t lenOfString=static_cast<int32_t>(value.size()+1);
    memcpy(rawdata+lenOfrawdata,&lenOfString,4);
    lenOfrawdata+=4;
    
    //push value:value's data
    memcpy(rawdata+lenOfrawdata,&value[0],value.size());
    lenOfrawdata+=value.size();

    //push value:end of value
    rawdata[lenOfrawdata]=END_OF_STRING;
    lenOfrawdata++;

    //push end of document
    rawdata[lenOfrawdata]=END_OF_DOCUMENT;
    lenOfrawdata++;
    
    //更新首部长度
    AFTER_APPEND;
}
void bson::document::append(std::string e_name,const char *value){
    int valueSize=0;
    while(value[valueSize]!='\0'){
        valueSize++;
    }

    grow(static_cast<int>(e_name.size()+valueSize+7));//-1+1+e_name.size()+1+4+valueSize+1+1
    
    rawdata[lenOfrawdata-1]=TYPE_STRING;

    PUSH_ENAME(bson::supportedElementType::STRING);

    int32_t lenOfString=static_cast<int32_t>(valueSize+1);
    memcpy(rawdata+lenOfrawdata,&lenOfString,4);
    lenOfrawdata+=4;

    memcpy(rawdata+lenOfrawdata,value,valueSize);
    lenOfrawdata+=valueSize;

    rawdata[lenOfrawdata]=END_OF_STRING;
    lenOfrawdata++;

    rawdata[lenOfrawdata]=END_OF_DOCUMENT;
    lenOfrawdata++;

    AFTER_APPEND;
}
void bson::document::append(std::string e_name,document& doc){
    grow(static_cast<int>(e_name.size()+doc.size()+2));//-1+1+e_name.size()+1+doc.size()+1

    rawdata[lenOfrawdata-1]=TYPE_DOCUMENT;
    
    if(doc.isArray()){
        PUSH_ENAME(bson::supportedElementType::ARRAY_DOCUMENT);
    }else{
        PUSH_ENAME(bson::supportedElementType::EMBEDDED_DOCUMENT);
    }

    //push doc's data
    memcpy(rawdata+lenOfrawdata,doc.rawdata,doc.lenOfrawdata);
    lenOfrawdata+=doc.lenOfrawdata;

    rawdata[lenOfrawdata]=END_OF_DOCUMENT;
    lenOfrawdata++;

    AFTER_APPEND;
}
void bson::document::append(std::string e_name,unsigned char *binary,int n){
    grow(static_cast<int>(e_name.size()+n+7));//-1+1+ename.size()+1+4+1+n+1

    rawdata[lenOfrawdata-1]=TYPE_BYTE_ARRAY;

    PUSH_ENAME(bson::supportedElementType::BYTE_ARRAY);
    
    //push length of byte array
    memcpy(rawdata+lenOfrawdata,&n,4);
    lenOfrawdata+=4;

    //push type of byte array
    rawdata[lenOfrawdata]=BYTE_ARRAY_TYPE;
    lenOfrawdata++;

    //push byte array
    memcpy(rawdata+lenOfrawdata,binary,n);
    lenOfrawdata+=n;

    rawdata[lenOfrawdata]=END_OF_DOCUMENT;
    lenOfrawdata++;

    AFTER_APPEND;
}
void bson::document::append(std::string e_name,bool boolean){
    grow(static_cast<int>(e_name.size()+2));//-1+1+ename.size()+1+1

    rawdata[lenOfrawdata-1]=TYPE_BOOL;
    
    PUSH_ENAME(bson::supportedElementType::BOOL);

    //push bool
    if(boolean){
        rawdata[lenOfrawdata]=UNSIGNED_CHAR_1;
    }else{
        rawdata[lenOfrawdata]=UNSIGNED_CHAR_0;
    }
    lenOfrawdata++;

    //push end of document
    rawdata[lenOfrawdata]=END_OF_DOCUMENT;
    lenOfrawdata++;

    AFTER_APPEND;
}
void bson::document::appendInt32(std::string e_name,int32_t value){
    grow(static_cast<int>(e_name.size()+6));//-1+1+ename.size()+1+4+1

    rawdata[lenOfrawdata-1]=TYPE_INT32;

    PUSH_ENAME(bson::supportedElementType::INT32);

    memcpy(rawdata+lenOfrawdata,&value,4);
    lenOfrawdata+=4;

    rawdata[lenOfrawdata]=END_OF_DOCUMENT;
    lenOfrawdata++;

    AFTER_APPEND;
}
void bson::document::appendInt64(std::string e_name,int64_t value){
    grow(static_cast<int>(e_name.size()+10));//-1+1+ename.size()+1+8+1

    rawdata[lenOfrawdata-1]=TYPE_INT64;

    PUSH_ENAME(bson::supportedElementType::INT64);

    memcpy(rawdata+lenOfrawdata,&value,8);
    lenOfrawdata+=8;

    rawdata[lenOfrawdata]=END_OF_DOCUMENT;
    lenOfrawdata++;

    AFTER_APPEND;
}

bson::document::~document(){
    delete[] rawdata;
}
