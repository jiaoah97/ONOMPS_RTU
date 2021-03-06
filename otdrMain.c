#include "common.h"
#include "otdr.h"
#include "sql.h"
#include "uploadCycTestData.h" 
#include "process.h"
#include "myModbus.h"
#include "checkip.h"
/***节点结构***/
/*
    SNO:光路号
    CM :局站号
    type:测试类型（具有优先级）
         --->type = 1 --------------> 点名测试（优先级最高）
         --->type = 2 --------------> 告警测试（优先级次高）
         --->type = 3 --------------> 周期测试（优先级最低）
    creat_time:节点创建的时间
*/
typedef struct otdrNode otdrNode;
struct otdrNode{
	int    SNo;                                 
        int    CM ;                                 
        int    type; 
        int    ModuleType;                               	                                                                                       
        time_t creat_time;  
        char   backIP[17];
        pid_t  masterPID;                        
	struct otdrNode *next;
};


/*全局变量*/
int flagNew=0;
int sem_id=0;
int modbus_sem_id=0;
int otdr_sem_id =0;
int n =0;                 //链表节点数
otdrNode *linkHead;       //链表头

static int   sigLock=0;
static queue *myQueue=NULL;
static pthread_mutex_t mutex;

static char serverIP[17];
/***插入有序节点****/
/*
     (1) 如果测试类型不同,插入到第一个类型比newNode大的节点之前.
     (2) 如果测试类型相同,按照插入时间的先后排序，先到先排，后到后排.
*/
otdrNode *insert(otdrNode *head,otdrNode *newNode)
{

        otdrNode *current=NULL;
        otdrNode *pre=NULL;

	current = head;
        while(current!=NULL&&current->type <= newNode-> type){   
            pre = current;
            current = current->next;
        }
        newNode->next = current;
        if(pre==NULL){  
            head = newNode; 
        }else{
            pre->next = newNode;
        }
        n++;
        return (head);
}

/***创建新链表***/
otdrNode *link_creat()
{
	otdrNode *head=NULL,*p1=NULL;
        head =   (otdrNode *) malloc (sizeof(otdrNode ));
        p1   =   (otdrNode *) malloc (sizeof(otdrNode ));
	head =   NULL;
        p1->SNo  =0;
        p1->CM   =0;
        p1->type =0;
        p1->ModuleType=0;
        p1->creat_time =0;
        strcpy(p1->backIP,"0.0.0.0");
        p1->masterPID=getpid();
	head = insert(head,p1);
        
	return(head);
}

/***删除节点***/
/*
   (1)以光路号SNo为索引
*/
otdrNode *delete(otdrNode *head,int SNo,int Type){
	otdrNode *p1=NULL,*p2=NULL;
	if(head==NULL){
		printf("There don't have node in OTDR testLink!\n");
		return(head);
	}
	p1= head;
	while(p1->SNo!=SNo && p1->next !=NULL){
		p2=p1;
		p1 =p1->next;
	}
	if(p1->SNo==SNo && p1->type==Type)
	{
		if(p1==head){
			head =p1->next;
		        free(p1);
                 }
		else
                 {
			p2->next =p1->next;
		        free(p1);
                        p1=NULL;
                 }
		n--;
	}else
		printf("Sorry,the SNo you want delete is not fount!\n");
	return(head);
}

/***查找结点***/
/*
   (1)以光路号SNo为索引
*/
otdrNode *findNode(otdrNode *head,int SNo)
{
	otdrNode * current=NULL;
        current = head;
        while(current!=NULL){
            if(current->SNo == SNo)  return current;

	     current = current->next;
        }
        return NULL;
}

/***判断链表是否为空***/
int isEmpty(otdrNode *head)
{
        return (head==NULL);
}

/***删除头节点***/
/*
    (1)链表非空的前提下才能删除
*/
otdrNode *deleteFirst(otdrNode *head )//O[1]
{
    otdrNode *temp=NULL;
    if (isEmpty(head)){
            return NULL;
     }
     temp = head;
     head = head->next;
     free(temp);
     temp=NULL;
     n--;
     return head;
}


/***输出头节点***/
/*
    (1)链表非空情况下才能输出
*/
otdrNode * outFirstnode(otdrNode *head)
{
        otdrNode *p0=NULL;
	if(head==NULL)
	     return(head);                               
        p0 = (otdrNode *) malloc (sizeof(otdrNode )); 
        p0->SNo           = head->SNo;
        p0->CM            = head->CM ;
        p0->type          = head->type;
        p0->ModuleType    = head->ModuleType;
        p0->creat_time    = head->creat_time;
        strcpy(p0->backIP , head->backIP);
        p0->masterPID     = head->masterPID;
	return(p0);
}
/***遍历链表***/
void outPutALL(otdrNode *head){
	otdrNode *p=NULL;
        int i=0;
	p= head;
	if(p==NULL){
		printf("There don't have node in OTDR testLink!\n");
		return ;
	}
	else
		printf("There are %d nodes on OTDR-TEST working link.\n",n);
	while(p!=NULL){
                printf("--------------------OTDR-Node:%d-------------------\n",++i);
                printf("|SNo:%4d                    TestType:%4d             \n",p->SNo,p->type);  
                printf("|CM :%4d                    ModuleType :%4d    \n",p->CM,p->ModuleType); 
                printf("|masterPID:%d              CreatTime:%d         \n",p->masterPID,p->creat_time); 
                printf("|backIP:%s                              \n",p->backIP); 
                printf("---------------------------------------------------\n\n");
		p=p->next;
	}
}

/***初始化链表****/
/*
    (1)创建一个空链表
    (2)删除头节点，形成空链表
*/
otdrNode * Init_CycleLink(void)
{
         otdrNode *head=NULL,*node=NULL;
         head = link_creat();
         head = delete(head,0,0);                                        
         return(head);
}

/***插入一个测试节点***/
/*
     (1)根据测试类型在数据库中查询光路（SNo）对应的局站号（rtuCM）
     (2)新建节点，设定好调度参数（测试类型，光路号，局站号，创建时间）
     (3)有序插入节点到调度链表
*/
otdrNode * insertTestNode(otdrNode *head,int type,int intSNo)          //插入数据库中状态为-1 的节点，并把状态修改为1
{
	 sqlite3 *mydb=NULL;
	 sql  *mysql=NULL;
         char **result = NULL;
         int  rednum =0;
	 int  rc=0;
         int  ModuleType=0,CM=0,i=0;
	 char SNo[5],ModNo[10],IP[17];
         otdrNode *node=NULL;


         node=(otdrNode *)malloc(sizeof(otdrNode));
         
         uint32tostring((uint32_t)intSNo,SNo);
         if(type!=1){
		 mysql = SQL_Create();
		 rc = sqlite3_open("/web/cgi-bin/System.db", &mydb);
		 if( rc != SQLITE_OK ){
			      printf( "Lookup SQL error\n");
                              return (head) ;
		 }
		 mysql->db = mydb;
		 if(type==3)
		    mysql->tableName     = "CycleTestSegnemtTable";
		 if(type==2)
		    mysql->tableName     = "AlarmTestSegmentTable";
	
		 mysql->mainKeyValue     = SNo;  
                 if( SQL_existIN_db(mysql)){
			 mysql->filedsName       = "rtuCM"; 
			 SQL_lookupPar(mysql,&result,&rednum);
			 CM =atoi(result[0]);
		         SQL_freeResult(&result,&rednum);
		         node->CM  =CM;
                

			 mysql->filedsName       = "IP01";
		         SQL_lookupPar(mysql,&result,&rednum);
		         strcpy(IP,result[0]);
		         SQL_freeResult(&result,&rednum);    

                         sprintf(ModNo,"%d",(atoi(SNo)-1)/8+1);
                         mysql->tableName     = "SubModuleTypeTable";
                         mysql->filedsName    = "ModuleType";
                         mysql->mainKeyValue  = ModNo;
                         SQL_lookupPar(mysql,&result,&rednum);
                         ModuleType = atoi(result[0]);
                         SQL_freeResult(&result,&rednum);

		         strcpy(node->backIP,IP);
		         node->masterPID=getpid();
                         node->ModuleType = ModuleType;

			 SQL_Destory(mysql);  
			 sqlite3_close(mydb); 
                 }else return (head);

         }else{
		 mysql = SQL_Create();
		 rc = sqlite3_open("/web/cgi-bin/System.db", &mydb);
		 if( rc != SQLITE_OK ){
			      printf( "Lookup SQL error\n");
                              return (head) ;
		 }

		 mysql->db = mydb;
		 mysql->tableName        = "NamedTestSegmentTable";
		 mysql->mainKeyValue     = SNo;   
                 if( SQL_existIN_db(mysql)){
			 mysql->filedsName       = "masterPID"; 
			 SQL_lookupPar(mysql,&result,&rednum);
			 node->masterPID =atoi(result[0]);
		         SQL_freeResult(&result,&rednum); 

                         sprintf(ModNo,"%d",(atoi(SNo)-1)/8+1);
                         mysql->tableName     = "SubModuleTypeTable";
                         mysql->filedsName    = "ModuleType";
                         mysql->mainKeyValue  =  ModNo;
                         SQL_lookupPar(mysql,&result,&rednum);
                         node->ModuleType     = atoi(result[0]);
                         SQL_freeResult(&result,&rednum);  
            
                 }else return (head);
		 SQL_Destory(mysql);  
		 sqlite3_close(mydb); 
            
                 node->CM  =0;
                 strcpy(node->backIP,"0.0.0.0");
         }

	 node->SNo =intSNo;
         node->type=type;
                      
         head=insert(head,node);     
	
              
         return(head);
}

/***加入新的测试节点***/
/*
   (1)信号附加参数是：type*100+SNo形式
      --->type=1 :加入一个点名测试节点,测试执行完毕，由main向BOA回复“1-OK”消息队列
      --->type=2 :加入一个告警测试节点,节点插入成功，向告警测试进程(alarmMain)发送“2-OK”消息队列
      --->type=3 :加入一个周期测试节点,节点插入成功，向周期测试进程(cycMain)发送“3-OK”消息队列
      --->type=4 :加入一个保护测试节点节点,节点插入成功，向保护测试进程(ProtectMasterMain)发送“4-OK”消息队列
   (2)同一类型的消息，按照信号到来的时间，依次插入测试链表
      --->可以解决多个客户端（WEB）向同一个光路，几乎同时进行点名测试。
*/

void  sigrecv_headle()
{
	       int type=0,value=0,ret,data;
       
               while(myQueue->count!=0){
          
		       ret=Queue_Delete(myQueue,&data);
		       type=data/100;
		       value =data%100;
		       printf("otdrMain(R): the int value is %d \n",data);
		       printf("type:%d,value:%d,",type,value);
		       switch(type){

				
				   case 1:{  
		                                    printf("插入点名测试节点\n");               
						    linkHead=insertTestNode(linkHead,1,value);
						    outPutALL(linkHead);     
						    //sendMessageQueue_Named(SMQstr,masterPID);         在主进程中回复
						    break;
					  }
				   case 2:{         
		                                    printf("障碍告警测试节点（普通）\n");                                        
						    linkHead=insertTestNode(linkHead,2,value);                   
						    outPutALL(linkHead);
						    sendMessageQueue_Function("2-OK",ALARM_MESSAGE_KEY );  
						    break;
					  }

				   case 4:{          
		                                    printf("障碍告警测试节点（保护）\n");                                          
						    linkHead=insertTestNode(linkHead,2,value);                   
						    outPutALL(linkHead);
						    sendMessageQueue_Function("4-OK",PROTECT_MESSAGE_KEY );  
						    break;
					  }

				   case 3:{       
		                                    printf("插入周期测试节点\n");                                          
						    linkHead=insertTestNode(linkHead,3,value);                   
						    outPutALL(linkHead);
						    sendMessageQueue_Function("3-OK",CYCLE_MESSAGE_KEY); 
					  }

				  default:break;
		      }
                      usleep(50000);
               }
     
}
/***otdr测试调度主进程***/
/*
    (1)  初始化链表
    (2)  创建信号机制，与BOA进程通信(SIG = SIGUSR1)
          -->SA_SIGINFO标志，表示信号附带的参数可以传递到信号处理函数中
          -->SA_RESTART标志自动重启被中断的慢系统调用.可以避免正在执行网络访问（eg:接收OTDR数据过程中,
             接收到调度进程发来信号时保持与OTDR的正常通信,不至于发生OTDR数据接收错误.
             eg:   145:recv error --> Interrupted system call
          -->信号安装，完成安装过程后，该进程就可以接收其他进程的消息，对方通过PID指定将信号发送到本进程
          -->信号处理程序（addNewtoLink）类似中断处理，可以异步接收信号，进程内部通过传递的参数执行不同的任务.
    (3)   执行调度程序
          --->A.获取待服务器节点（头节点）
          --->B.查询光路测试参数
          --->C.执行OTDR测试
          --->D.根据测试类型回传数据文件
                点名测试：向BOA发送消息队列，由BOA回传.
                周期、告警测试:调用HTTPClient，向指定服务器回传.
          --->E.测试完毕，删除当前测试节点.
          --->F.循环调度下个节点(to A).
*/

void work_line(void)
{
        otdrNode *p1=NULL;
        int SNo=0,i;
        int intCM=0;
        int ModuleType,type=0;
        int otdrSWFlag=-1;
        pid_t masterPID;
        otdr * testPar=NULL;
        modbus_t * mb=NULL ;
        backData *bData=NULL; 
        char *SMQstr=NULL;
       
        linkHead=Init_CycleLink();
        printf("-----------------work_line Here\n");
        /*执行调度程序*/
        while(1){
            sigrecv_headle();
            p1=outFirstnode(linkHead);            
            if(p1!=NULL){
		   SNo     = p1->SNo; 
		   intCM   = p1->CM;
	  	   type    = p1->type;
		   masterPID = p1->masterPID;
                   ModuleType =p1->ModuleType;
		   testPar = lookupParm(SNo,type); 
		   if(testPar->haveParm == 1){                     
			   printf("NowTime:%ld,Type:%d\n" ,getLocalTimestamp(),type);
			   printf("SNo      -uint -[%d]\n",SNo);
			   printf("P01      -uint -[%d]\n",testPar->MeasureLength_m);
			   printf("P02      -uint -[%d]\n",testPar->PulseWidth_ns);
			   printf("P03      -uint -[%d]\n",testPar->Lambda_nm);
			   printf("P04      -uint -[%d]\n",testPar->MeasureTime_ms);
			   printf("P05      -float-[%f]\n",testPar->n);
			   printf("P06      -float-[%f]\n",testPar->NonRelectThreshold);
			   printf("P07      -float-[%f]\n",testPar->EndThreshold);
		           printf("masterPID-short-[%d]\n",masterPID);	

			   if(!setModbus_P())                                                //P
				 exit(EXIT_FAILURE); 
			   otdrSWFlag=-1;
			   mb =newModbus(MODBUS_DEV,MODBUS_BUAD);
		           otdrSWFlag = doOtdrSwitch(mb,SNo,onlyOne_Moudle,ModuleType); 
                           freeModbus(mb);
			   if(!setModbus_V())                                                //V
				 exit(EXIT_FAILURE); 
                           //otdrSWFlag=0;
			   if(otdrSWFlag==0){ 
				     if(type==2)
					printf("Excess OTDR Test ------------------------------------------------------------------>障碍告警测试   SNo=%d\n",SNo);
				     if(type==3)
					printf("Excess OTDR Test ------------------------------------------------------------------>周期测试       SNo=%d\n",SNo);
				     if(type==1)
					printf("Excess OTDR Test ------------------------------------------------------------------>点名测试       SNo=%d masterPID:%d\n",SNo,masterPID);
				     OtdrTest(testPar);	  
				     if(type == 1){
				          SMQstr = (char*)malloc(sizeof(char)*20);
				          sprintf(SMQstr,"1-OK-%d",masterPID);
					  sendMessageQueue_Named(SMQstr,masterPID);                //根据主控进程进程号区分消息类型
				          free(SMQstr);
				          SMQstr=NULL;
				     }else{  
				     
					  bData=backData_Create();
					  bData->otdrPar =testPar;
					  strcpy(bData->backIP,p1->backIP);
					  upload(bData,SNo,intCM,type);
					  backData_Destory(bData); 
                                      
				     } 
                                    // remove(en_ORDRDATA);                             
				     printf("-------OTDR--Test-------\n");
			  	     linkHead = delete(linkHead,SNo,type);                                          
			             }else{
                                         linkHead = delete(linkHead,SNo,type);
                                     }
		  }else{
                          if(testPar->haveParm == 0){
                                 linkHead = delete(linkHead,SNo,type);
                          }
		  }
		 OTDR_Destory(testPar); 
                 outPutALL(linkHead);	 
 
		 if(p1!=NULL){
		          free(p1); 
			  p1=NULL;
	         }	
            }
            usleep(10000);
        }
}

void addNewtoLink(int signum,siginfo_t *info,void *myact);
int  main( int argc ,char **argv ){

        if(argc<2){
             printf("请输入服务器地址，用于检查RTU链接! otdrMain [ServerIP]\n");
             return -1;
        }else{

             strcpy(serverIP,argv[1]);
             if(!PM_Accsee_IP("eth0",serverIP)){
                printf("服务器地址 ServerIP:%s  可达\n",argv[1]);
             }else{
                 printf("服务器地址 ServerIP:%s 不可达,重新设置\n",argv[1]);
                 return -1;
             }
            
        }

        sem_id        = semget((key_t)1234, 1, (4777 | IPC_CREAT));                        //创建数据库信号量 :每一个需要用到信号量的进程,在第一次启动的时候需要初始化信号量
        modbus_sem_id = semget((key_t)5678, 1, (4777 | IPC_CREAT));                        //每一个需要用到信号量的进程,在第一次启动的时候需要初始化信号量 
        if(!set_semvalue())                                                                //程序第一次被调用，初始化信号量
        {  
            printf("Failed to initialize semaphore\n");  
            exit(EXIT_FAILURE);  
        }  

        if(!setModbusPV())                                                                  //程序第一次被调用，初始化信号量
        {  
              printf("Failed to initialize modbus_semaphore\n");  
              exit(EXIT_FAILURE); 
        } 	
        /*初始化信号机制（IPC）(BOA)*/
        struct sigaction act;
        int sig;
        sig= SIGRTMIN;
        sigemptyset(&act.sa_mask);
        act.sa_sigaction=addNewtoLink;
        act.sa_flags=SA_SIGINFO|SA_RESTART;                                                                                                                                                               
        if(sigaction(sig,&act,NULL)<0){                              
            printf("install sigal error\n");
        }
        myQueue = Queue_Initiate(); 

        work_line();

}


void configNetwork(){
        int flag=0,fd=0;

	char newIP[16];
	char oldIP[16];
	char oldMask[16];
	char str[30];

	get_ip(newIP);
						 
	if(0!=PM_Accsee_IP("eth0",serverIP)){
		if ((fd = open ("/web/cgi-bin/fiberMointor.tmp" , O_RDWR | O_CREAT)) < 0){
			perror ("Error opening file");
		}else{
			lseek(fd,0,SEEK_SET);			 
			read(fd,oldIP,16);
			printf("Back to Old IP:%s",oldIP);  
			read(fd,oldMask,16); 
			printf("Back to Old Mask:%s",oldMask); 
		}
		close (fd);                                  
		set_ip(oldIP);
	}else{
		printf("Set to New IP:%s",newIP); 
		if ((fd = open ("/web/cgi-bin/fiberMointor.conf" , O_RDWR | O_CREAT)) < 0){
			perror ("Error opening file");
		}else{
			sprintf(str,"eth0 %s",newIP);
			lseek(fd,0,SEEK_SET);			 
			write(fd,str,strlen(str));   
		}
		close (fd);                           
	} 
	

}
void doReboot(){
          
		printf("Start Reboot System......\n");  
                sendMessageQueue_Named("0-OK-38",38);
		execl("/web/cgi-bin/reboot.sh","reboot.sh");      				 	   

}
void addNewtoLink(int signum,siginfo_t *info,void *myact){
      while(sigLock==1);
      sigLock=1;
      Queue_Append(myQueue,info->si_int);
      if(info->si_int == 16) configNetwork();//网络配置
      if(info->si_int == 38) doReboot();     //重启RTU  
      sigLock=0; 
}


