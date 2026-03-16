#include "protocol.h"

//构造函数
PDU *makePDU(unsigned int uiMsglen)
{
    unsigned int uiPDUlen = sizeof(PDU)+uiMsglen;
    PDU* pdu = (PDU*)malloc(uiPDUlen);
    if(!pdu){
        exit(EXIT_FAILURE);
    }

    //清空
    memset(pdu, 0, uiPDUlen);
    pdu->uiPDUlen = uiPDUlen;
    pdu->uiMsglen = uiMsglen;

    return pdu;
}
