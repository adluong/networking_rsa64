//This file provides a simple key exchange protocol

#include "mbed.h"
#include "string.h"

#include "ARQ_FSMevent.h"
#include "ARQ_msg.h"
#include "ARQ_timer.h"
#include "ARQ_LLinterface.h"
#include "ARQ_parameters.h"


//FSM state -------------------------------------------------
#define MAINSTATE_IDLE              0
#define MAINSTATE_TX                1
#define WAIT_ACK                    2
// #define uint64_t unsigned long long int
//GLOBAL variables (DO NOT TOUCH!) ------------------------------------------
//serial port interface
Serial pc(USBTX, USBRX);

//state variables
uint8_t main_state = MAINSTATE_IDLE; //protocol state

//source/destination ID
uint8_t endNode_ID=1;
uint8_t dest_ID=0;

//PDU context/size
uint8_t arqPdu[200];
uint8_t arqPduAck[200];
uint8_t pduSize;
uint8_t pduSizeAck;

//SDU (input)
uint8_t originalWord[200];
uint8_t wordLen=0;

//ARQ parameters -------------------------------------------------------------
uint8_t seqNum = 0;     //ARQ sequence number
uint8_t retxCnt = 0;    //ARQ retransmission counter
uint8_t arqAck[5];      //ARQ ACK PDU

uint8_t rtm = 0;

//my parameters
unsigned long long int seed;
uint8_t seed_8[200];
unsigned long long int base = 922337203685477580U;
unsigned long long int order = 922337203685477588U;
unsigned long long int token;
uint8_t token_8[200];
unsigned long long int token_in;
unsigned long long int key;
bool isKeyExchange = false;

//application event handler : generating SDU from keyboard input
void arqMain_processInputWord(void){
    
    char c = pc.getc();
    if (main_state == MAINSTATE_IDLE &&
        !arqEvent_checkEventFlag(arqEvent_dataToSend))
    {
        if (c == '\n' || c == '\r')
        {
            originalWord[wordLen++] = '\0';
            arqEvent_setEventFlag(arqEvent_dataToSend);
            pc.printf("'%s'\n", originalWord);
        }
        else
        {
            originalWord[wordLen++] = c;
            if (wordLen >= ARQMSG_MAXDATASIZE-1)
            {
                originalWord[wordLen++] = '\0';
                arqEvent_setEventFlag(arqEvent_dataToSend);
                pc.printf("\n max reached! word forced to be ready :::: %s\n", originalWord);
            }
        }
    }
}
unsigned long long int mod_mul(unsigned long long int a, unsigned long long int b, unsigned long long int mod){
    unsigned long long int res = 0; // Initialize result
    a %= mod;
    while (b) {
        // If b is odd, add a with result
        if (b & 1)
            res = (res + a) % mod;
        a = (2 * a) % mod;
        b >>= 1; // b = b / 2
    }
    return res;
}
unsigned long long int mod_power(unsigned long long int x, unsigned long long int y, unsigned long long int p){
    // Initialize answer
    unsigned long long int res = 1;
    // Check till the number becomes zero
    while (y > 0) {
        // If y is odd, multiply x with result
        if (y % 2 == 1)
            res = mod_mul(res,x,p);
        // y = y/2
        y = y >> 1;
        // Change x to x^2
        x = mod_mul(x,x,p);
    }
    return res % p;
}
void print_menu(){
    pc.printf("=========================================================\n");
    pc.printf("=                        START!                         =\n");
    pc.printf("=            Computer Network Final Project.            =\n");
    pc.printf("=               Luong Duc Anh - 202131033               =\n");
    pc.printf("=========================================================\n");
}
unsigned long long int atollu(uint8_t *num){
    unsigned long long int buf;
    unsigned long long int total = 0;
    uint8_t l;
    uint8_t n;
    uint8_t j;
    const char* str = (const char*)num;

    l = strlen(str);
    for(int i = l; i >= 0; i--){
        j = l-i;
        buf = pow(10,i-1);
        n = num[j]-48;
        total += n*buf;
    }

    return total;
}
void llutoa(unsigned long long int num, uint8_t *des){
    uint8_t j;
    uint8_t l = 0;
    unsigned long long int buf;
    unsigned long long int p = 1;

    while(1){
        buf = num / p;
        if (buf == 0){
            break;
        }
        else{
            l++;
            p *= 10;
        }
    }
    for(int i = l-1; i >= 0; i--){
        j = l-1-i;
        buf = (unsigned long long int)pow(10,i);
        des[j] = num/buf + 48;
        num = num%buf;
    }
}
void get_seed(){
    pc.printf("Enter seed: ");
    pc.scanf("%llu", &seed);
    pc.printf("%llu\n", seed);
}
void generate_token(){
    if(seed != 0){
        token = mod_power(base, seed, order);
        llutoa(token, token_8);
    }
    else{
        printf("------------------------------------------------------------\n");
        printf("Invalid seed (seed cannot be 0)!\n");
        printf("------------------------------------------------------------\n");
    };
}
void generate_key(){
    key = mod_power(token_in, seed, order);
    pc.printf("........................................................\n");
    pc.printf(". Key exchange success!\n");
    pc.printf(". g = %llu\n. p = %llu\n. Seed_%i = %llu\n. Token_%i = %llu\n. Received Token_%i = %llu\n. The secret key = %llu\n",
                base, order, endNode_ID, seed, endNode_ID, token, dest_ID, token_in, key);
    pc.printf("........................................................\n");
    isKeyExchange = true;

}

void get_id(){
    pc.printf(":: ID for this node : ");
    pc.scanf("%d", &endNode_ID);
    pc.printf("%i\n",endNode_ID);
    pc.printf(":: ID for the destination : ");
    pc.scanf("%d", &dest_ID);
    pc.printf("%i\n",dest_ID);
}

void init(){
    arqLLI_initLowLayer(endNode_ID);
    pc.attach(&arqMain_processInputWord, Serial::RxIrq);
}

//FSM operation implementation ------------------------------------------------
int main(void){
    uint8_t flag_needPrint=1;
    uint8_t prev_state = 0;

    //initialization
    pc.printf("------------------ ARQ protocol starts! --------------------------\n");
    arqEvent_clearAllEventFlag();
    arqEvent_setEventFlag(arqEvent_keyExchange);

    get_id();
    print_menu();
    get_seed();
    init();
    generate_token();


    
    arqEvent_setEventFlag(arqEvent_dataToSend);
    pduSize = arqMsg_encodeData(arqPdu, token_8, seqNum, strlen((const char*)token_8));
    arqLLI_sendData(arqPdu, pduSize, dest_ID);
    pc.printf("| [MAIN] sending token to %i (msg: %s, seq:%i)\n", dest_ID, token_8, (seqNum)%ARQMSSG_MAX_SEQNUM);
    seqNum++;
    main_state = WAIT_ACK;
    arqEvent_clearEventFlag(arqEvent_dataToSend);

    while(1)
    {
        //debug message
        if (prev_state != main_state)
        {
            debug_if(DBGMSG_ARQ, "[ARQ] State transition from %i to %i\n", prev_state, main_state);
            prev_state = main_state;
        }


        //FSM should be implemented here! ---->>>>
        switch (main_state)
        {
            case MAINSTATE_IDLE: //IDLE state description

                //================================================
                //RECEIVE DATA
                //================================================
                if (arqEvent_checkEventFlag(arqEvent_dataRcvd)) //if data reception event happens
                {
                    //Retrieving data info.
                    uint8_t srcId = arqLLI_getSrcId();
                    uint8_t* dataPtr = arqLLI_getRcvdDataPtr();
                    uint8_t size = arqLLI_getSize();
                    uint8_t rcd_seq = arqMsg_getSeq(dataPtr);
                    
                    //rcd_seq = 0 implies that this data is the token
                    //if received data is a token, generate a key from it.
                    if(rcd_seq == 0){
                        pc.printf("\n-------------------------------------------------\n| Received Token_in from %i : %s (length:%i, seq:%i)\n", 
                                srcId, arqMsg_getWord(dataPtr), size, arqMsg_getSeq(dataPtr));
                        token_in = atollu(arqMsg_getWord(dataPtr));    
                        generate_key();
                        }
                    else{ //if received data is not a token, treat it as normal data
                        pc.printf("\n-------------------------------------------------\n| RCVD from %i : %s (length:%i, seq:%i)\n", 
                        srcId, arqMsg_getWord(dataPtr), size, arqMsg_getSeq(dataPtr));
                        }
                    
                    //send ACK
                    pduSizeAck = arqMsg_encodeAck(arqPduAck, rcd_seq);
                    arqEvent_setEventFlag(arqEvent_dataToSend);
                    arqLLI_sendData(arqPduAck, pduSizeAck, srcId);
                    arqEvent_clearEventFlag(arqEvent_dataToSend);
                    pc.printf("| ACK for seq %i is sent.\n-------------------------------------------------\n", rcd_seq);                    

                    main_state = MAINSTATE_IDLE;
                    flag_needPrint = 1;

                    arqEvent_clearEventFlag(arqEvent_dataRcvd);
                }

                //================================================
                //SEND DATA
                //================================================
                else if (arqEvent_checkEventFlag(arqEvent_dataToSend)) //if data needs to be sent (keyboard input)
                {
                    pduSize = arqMsg_encodeData(arqPdu, originalWord, seqNum, wordLen);
                    arqLLI_sendData(arqPdu, pduSize, dest_ID);
                    pc.printf("| [MAIN] sending to %i (msg: %s, seq:%i)\n", dest_ID, originalWord, (seqNum)%ARQMSSG_MAX_SEQNUM);
                    seqNum++;
                    main_state = WAIT_ACK;
                }

                //================================================
                //WAITING FOR WORDS
                //================================================
                else if (flag_needPrint == 1)
                {
                    pc.printf("\n-------------------------------------------------\n| Give a word to send: ");
                    flag_needPrint = 0;
                }     

                break;

            case MAINSTATE_TX: //IDLE state description
                if (arqEvent_checkEventFlag(arqEvent_dataTxDone)) //data TX finished
                {
                    main_state = MAINSTATE_IDLE;
                    // pc.printf("state mainstate_idle\n");
                    arqEvent_clearEventFlag(arqEvent_dataTxDone);
                }

                break;

            case WAIT_ACK:
                //start the timer
                arqTimer_startTimer();
                //keep waiting for the timer
                while(arqTimer_getTimerStatus()){       
                    //if ACK is received in time:
                    //clear the timer and go back to mainstate
                    if(arqEvent_checkEventFlag(arqEvent_ackRcvd)){
                        // pc.printf("event received ack\n");
                        uint8_t* ACKptr = arqLLI_getRcvdDataPtr();
                        if(!arqMsg_checkIfAck(ACKptr)) break;
                        pc.printf("| Received ACK: srcID = %i, seq = %i.\n-------------------------------------------------\n", arqLLI_getSrcId(), arqMsg_getSeq(ACKptr));
                        arqTimer_stopTimer();
                        wordLen = 0;
                        arqEvent_clearEventFlag(arqEvent_dataToSend);
                        arqEvent_clearEventFlag(arqEvent_ackRcvd);
                        flag_needPrint = 1;
                        main_state = MAINSTATE_TX;
                        // pc.printf("state mainstate_tx\n");
                    }
                    //if data is received while waiting for ACK:
                    //if it is a token: generate key from it.
                    //else: treat it as normal data
                    //note that, after sending ACK, the state still remains WAIT_ACK because ACK is not received yet.
                    else if(arqEvent_checkEventFlag(arqEvent_dataRcvd)){
                        uint8_t srcId = arqLLI_getSrcId();
                        uint8_t* dataPtr = arqLLI_getRcvdDataPtr();
                        uint8_t size = arqLLI_getSize();
                        uint8_t rcd_seq = arqMsg_getSeq(dataPtr);
                        if(rcd_seq == 0){
                            pc.printf("\n-------------------------------------------------\n| Received Token_in from %i : %s (length:%i, seq:%i)\n", 
                                    srcId, arqMsg_getWord(dataPtr), size, arqMsg_getSeq(dataPtr));
                                    token_in = atollu(arqMsg_getWord(dataPtr));
                                    generate_key();
                        }
                        else{
                            pc.printf("\n-------------------------------------------------\n| RCVD from %i : %s (length:%i, seq:%i)\n", 
                        srcId, arqMsg_getWord(dataPtr), size, arqMsg_getSeq(dataPtr));
                        }

                        //send ACK for the received data
                        pduSizeAck = arqMsg_encodeAck(arqPduAck, rcd_seq);
                        arqEvent_setEventFlag(arqEvent_dataToSend);
                        arqLLI_sendData(arqPduAck, pduSizeAck, srcId);
                        arqEvent_clearEventFlag(arqEvent_dataToSend);
                        pc.printf("| ACK for seq %i is sent.\n-------------------------------------------------\n", rcd_seq);                    

                        arqEvent_clearEventFlag(arqEvent_dataRcvd);
                    }
                }
                // if time out, retransmit the packet
                if(arqEvent_checkEventFlag(arqEvent_arqTimeout)){
                    if(rtm < ARQ_MAXRETRANSMISSION-6){
                        //restranmit the packet
                        arqEvent_setEventFlag(arqEvent_dataToSend);
                        arqLLI_sendData(arqPdu, pduSize, dest_ID);
                        arqEvent_clearEventFlag(arqEvent_dataToSend);
                        pc.printf("| Retransmitting to %i\n", dest_ID);

                        main_state = WAIT_ACK;
                        arqEvent_clearEventFlag(arqEvent_arqTimeout);
                        //increase the restranmission counter
                        rtm++;
                    }
                    else{ //if the packet is retransmitted 4 times, stop restranmitting and go back to mainstate_idle
                        arqEvent_clearEventFlag(arqEvent_dataToSend);
                        pc.printf("Packet transmitting failed. No ACK received.\n");
                        arqEvent_clearEventFlag(arqEvent_arqTimeout);
                        wordLen = 0;
                        main_state = MAINSTATE_IDLE;
                        flag_needPrint = 1;
                        rtm = 0;
                    }
                }
                break;
            default :
                break;
        }
    }
}
