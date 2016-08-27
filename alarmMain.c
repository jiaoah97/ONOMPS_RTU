#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <signal.h>
#include "common.h"
#include "defaulttest.h"
#include "process.h"
#include "sql.h"
#include "otdr.h"
#include "uploadCycTestData.h" 

/***测试链表节点结构***/ //LinkA
/*
    SNO:光路号
    CM :局站号
    ANo:告警组号（优先级）
    PowerGate     :光功率门限，低于此门限产生告警
    protectFlag   :光保护标识。0：不存在保护光路。1：存在保护光路
    fristAlarmFlag:首次告警标识：首次产生告警，置位为1，告警消失：置位为0。
    nextAlarmTime :告警未消失时，下次向服务器上传告警信息的时间
    alarmClick    :上传告警的周期
*/

typedef struct checkNode checkNode;
struct checkNode{
	int    SNo;
        int    CM;
	int    ANo;
        float  PowerGate;
        int    protectFlag;
        int    fristAlarmFlag;
        time_t nextAlarmTime;
        time_t alarmClick;
	struct checkNode *next;
};

/***告警链表节点***/   //LinkB
/*
    SNO:光路号
    CM :局站号
    ANo:告警组号（优先级）
    Order:节点序列号，按优先级排序依据 Order = ANo*100 + SNo
*/
typedef struct alarmNode alarmNode;
struct alarmNode{
	int    SNo;
        int    CM;
	int    ANo;
        int    Order;
	struct alarmNode *next;
};


/*全局变量*/
int sem_id=0;                      //信号量ID（数据库互斥访问）                             
int flagNewA=0;                    //有新的测试节点加入标识
int flagNewB=0;                    //有新的异常节点加入标识
checkNode *linkHead_check_A;       //测试节点链表头
alarmNode *linkHead_alarm_B;       //异常节点链表头
int num_A =0;                      //测试链表节点数
int num_B =0;                      //异常链表节点数

/*****************************LinkA-checkLink************************************/
/***插入测试节点****/
/*
     (1) 尾插法.
*/
checkNode *insert_A(checkNode *head,checkNode *newNode)
{
        checkNode *current;
        checkNode *pre;
        current = (checkNode *) malloc (sizeof(checkNode ));
        pre = (checkNode *) malloc (sizeof(checkNode ));
	pre     = NULL;
	current = head;
        while(current!=NULL){
            pre = current;
            current = current->next;
        }
        newNode->next = current;
        if(pre==NULL){  
            head = newNode; 
        }else{
            pre->next = newNode;
        }
        num_A++;
        return (head);
}
/***创建新链表***/
checkNode *link_creat_A(){
	checkNode *head,*p1;
        head = (checkNode *) malloc (sizeof(checkNode ));
        p1 =   (checkNode *) malloc (sizeof(checkNode ));
	head =NULL;
        p1->SNo           =0;
        p1->CM            =0;
        p1->ANo           =0;
        p1->PowerGate     =0;
        p1->protectFlag   =0;
        p1->fristAlarmFlag=0;
        p1->nextAlarmTime =0;
        p1->alarmClick    =0;

	head = insert_A(head,p1);
	return(head);
}

/***判断链表是否为空***/
int isEmpty_A(checkNode *head){
        return (head==NULL);
}

/***删除头节点***/
/*
    (1)链表非空的前提下才能删除
*/
checkNode *deleteFirst_A(checkNode *head ){
        if (isEmpty_A(head)){
            return NULL;
        }
        checkNode *temp;
        temp = (checkNode *) malloc (sizeof(checkNode ));
        temp = head;
        head = head->next;
        num_A--;
        return temp;
    }

/***输出头节点***/
/*
    (1)链表非空情况下才能输出
*/
checkNode * outFirstnode_A(checkNode *head)
{
        checkNode *p0;
	if(head==NULL){
		return(head);                               
	}
        p0 = (checkNode *) malloc (sizeof(checkNode ));     
        p0->SNo           = head->SNo;
        p0->CM            = head->CM;
        p0->ANo           = head->ANo;
        p0->PowerGate     = head->PowerGate;
        p0->protectFlag   = head->protectFlag;
        p0->fristAlarmFlag= head->fristAlarmFlag;
        p0->nextAlarmTime = head->nextAlarmTime;
        p0->alarmClick    = head->alarmClick;
	return(p0);
}

/***删除节点***/
/*
   (1)以光路号SNo为索引
*/
checkNode *delete_A(checkNode *head,int SNo){
	checkNode *p1,*p2;
	if(head==NULL){
		printf("This is a void execl");
		return(head);
	}
	p1= head;
	while(p1->SNo!=SNo && p1->next !=NULL){
		p2=p1;
		p1 =p1->next;
	}
	if(p1->SNo==SNo)
	{
		if(p1==head){
			head =p1->next;
		        free(p1);
                 }
		else
                 {
			p2->next =p1->next;
		        free(p1);
                 }
		num_A--;
	}else
		printf("Sorry,the SNo you want to delete is not fount!\n");
	return(head);
}

/***查找结点***/
/*
   (1)以光路号SNo为索引
*/
checkNode *findNode_A(checkNode *head,int SNo)
{
	checkNode * current;
        current = head;
        while(current!=NULL){
            if(current->SNo == SNo)
                return current;
	     current = current->next;
        }
        return NULL;
}

/***遍历链表***/
void outPutALL_A(checkNode *head){
	checkNode *p;
	p= head;
	if(p==NULL){
		printf("This is a void excel!\n");
		return ;
	}
	else
		printf("There are %d lines on testing_A:\n",num_A);
	while(p!=NULL){
		printf("SNo:%d,rtuCM:%d,ANo:%d,PowerGate:%f,protectFlag:%d,fristAlarmFlag:%d,nextAlarmTime:%ld,alarmClick:%ld\n"
                       ,p->SNo,p->CM,p->ANo,p->PowerGate,p->protectFlag,p->fristAlarmFlag,p->nextAlarmTime, p->alarmClick);
                p=p->next;
	}
}




/***************************LinkB-alarmLink************************************/
/***插入异常有序节点****/
/*
     (1) 根据故障优先级大小排序.
*/
alarmNode *insert_B(alarmNode *head,alarmNode *newNode)
{

        alarmNode *current;
        alarmNode *pre;
        current = (alarmNode *) malloc (sizeof(alarmNode ));
        pre = (alarmNode *) malloc (sizeof(alarmNode ));
	pre     = NULL;
	current = head;
        while(current!=NULL&&current->Order < newNode->Order){
            pre = current;
            current = current->next;
        }
        newNode->next = current;
        if(pre==NULL){  
            head = newNode; 
        }else{
            pre->next = newNode;
        }
        num_B++;
        return (head);
}

/***创建新链表***/
alarmNode *link_creat_B(){
	alarmNode *head,*p1;
        head = (alarmNode *) malloc (sizeof(alarmNode ));
        p1   = (alarmNode *) malloc (sizeof(alarmNode ));
	head =NULL;
        p1->SNo =0;
        p1->CM =0;
        p1->ANo =0;
        p1->Order =0;
	head = insert_B(head,p1);
	return(head);
}

/***判断链表是否为空***/
int isEmpty_B(alarmNode *head){
        return (head==NULL);
}

/***删除头节点***/
/*
    (1)链表非空的前提下才能删除
*/
alarmNode *deleteFirst_B(alarmNode *head ){
        if (isEmpty_B(head)){
            return NULL;
        }
        alarmNode *temp;
        temp = (alarmNode *) malloc (sizeof(alarmNode ));
        temp = head;
        head = head->next;
        num_B--;
        return temp;
    }

/***输出头节点***/
/*
    (1)链表非空情况下才能输出
*/
alarmNode * outFirstnode_B(alarmNode *head)
{
        alarmNode *p0;
	if(head==NULL){
		return(head);                               
	}
        p0 = (alarmNode *) malloc (sizeof(alarmNode ));        
        p0->SNo           = head->SNo;
        p0->CM            = head->CM;
        p0->ANo           = head->ANo;
        p0->Order         = head->Order;
	return(p0);
}

/***删除节点***/
/*
   (1)以光路号SNo为索引
*/
alarmNode *delete_B(alarmNode *head,int SNo){
	alarmNode *p1,*p2;
	if(head==NULL){
		printf("This is a void execl");
		return(head);
	}
	p1= head;
	while(p1->SNo!=SNo && p1->next !=NULL){
		p2=p1;
		p1 =p1->next;
	}
	if(p1->SNo==SNo)
	{
		if(p1==head){
			head =p1->next;
		        free(p1);
                 }
		else
                 {
			p2->next =p1->next;
		        free(p1);
                 }
		num_B--;
	}else
		printf("Sorry,the SNo you want delete is not fount!\n");
	return(head);
}

/***查找结点***/
/*
   (1)以光路号SNo为索引
*/
alarmNode *findNode_B(alarmNode *head,int SNo)
{
	alarmNode * current;
        current = head;
        while(current!=NULL){
            if(current->SNo == SNo)
                return current;
	     current = current->next;
        }
        return NULL;
}

/***遍历链表***/
void outPutALL_B(alarmNode *head){
	alarmNode *p;
	p= head;
	if(p==NULL)
	   return ;
        else {
          printf("There are %d lines on testing_B.\n",num_B); 
	  while(p!=NULL){
		printf("SNo:%d,rtuCM:%d,Level:%d,Order:%d\n",p->SNo,p->CM,p->ANo,p->Order);
		p=p->next;
	  }
       }
}



/**************************************************************************/
/***初始化测试链表****/
/*
    (1)创建一个空链表
    (2)将数据库障碍告警测试表中具有启动标识（status=1）的光路，加入初始化链表。
          --->光路状态为1 表示正在进行障碍告警测试表，因此在初始化过程中需要将其加入调度链表.
          --->保证每次启动，需要进程测试的节点能加入链表（例如，停机重启之后).
*/
checkNode * InitA_CycleLink(void)
{
	 sqlite3 *mydb;
	 char *zErrMsg = 0,*SNo;
	 int rc,i,SN=0;
	 sql *mysql;
	 char resultSNo[64][5];
         char **result = NULL;
         checkNode *head,*node;
         time_t T3,T4;
         head = link_creat_A();
         head = delete_A(head,0);                                     
         uint32_t ANo,protectFlag;
         float PowerGate;
         int    CM=0;
	 SNo = (char *) malloc(sizeof(char)*5);
	 mysql = SQL_Create();
	 rc = sqlite3_open("/web/cgi-bin/System.db", &mydb);
	 if( rc != SQLITE_OK ){
		      printf( "Lookup SQL error: %s\n", zErrMsg);
		      sqlite3_free(zErrMsg);
		   }
	 mysql->db = mydb;
	 mysql->tableName   =  "AlarmTestSegmentTable";	
         mysql->filedsValue =  "1";                                
         mysql->filedsName  =  "Status";
         SN=SQL_findSNo(mysql,resultSNo);
         if(SN>0){
		for(i =0 ;i<SN;i++)
		    {
		        printf("SNo:%s",resultSNo[i]);
		        strcpy(SNo,resultSNo[i]);
		        mysql->mainKeyValue  = SNo;

		        mysql->filedsName    = "rtuCM"; 
		        rc= SQL_lookup(mysql,&result);
                        CM =atoi(result[0]);
		        printf("CM:%d\n",CM);

		        mysql->filedsName    = "Level"; 
		        rc= SQL_lookup(mysql,&result);
                        ANo =atoi(result[0]);
		        printf("ANo:%d\n",ANo);

		        mysql->filedsName    = "AT06"; 
		        rc= SQL_lookup(mysql,&result);
                        PowerGate=atof(result[0]);
		        printf("PowerGate:%f\n",PowerGate);

		        mysql->filedsName    = "protectFlag"; 
		        rc= SQL_lookup(mysql,&result);
                        protectFlag=atoi(result[0]);
		        printf("protectFlag:%d\n",protectFlag);


		        mysql->filedsName    = "T3"; 
		        rc= SQL_lookup(mysql,&result);
                        T3 =computTime(result[0]);
		        printf("T3:%ld\n",T3);

		        mysql->filedsName    = "T4"; 
		        rc= SQL_lookup(mysql,&result);
                        T4= computTime(result[0]);
		        printf("T4:%ld\n",T4);


			node=(checkNode *)malloc(sizeof(checkNode));
			node->SNo           = atoi(SNo);
                        node->CM            = CM;
			node->ANo           = ANo;
			node->PowerGate     = PowerGate;
			node->protectFlag   = protectFlag;
			node->fristAlarmFlag= 0;
			node->nextAlarmTime = getLocalTimestamp();
			node->alarmClick    = T3;
                        head=insert_A(head,node);  
		    }
         }
         free(SNo);
	 SQL_Destory(mysql);  
	 sqlite3_close(mydb);

	 if(result != NULL)
	   {
	     if(result[0] != NULL)
		{
			free(result[0]);
			result[0] = NULL;
		}

		free(result);
		result = NULL;
	  }
         return(head);
}

alarmNode * InitB_CycleLink(void)
{
         alarmNode *head,*node;
         head = link_creat_B();
         head = delete_B(head,0); 
         return(head);
}
/***刷新状态***/
/*
      (1)将数据库中状态为“-1”和“-2”的状态光路，修改为“1”和“2”。
          --->防止BOA单方面修改状态而并没有通过插入节点程序更新状态，导致后台周期测试调度程序没有发现，或由于意外情况发生，没有及时处理的光路。
*/
void flushWaitingSNo(void)
{
      	 sqlite3 *mydb;
	 char *zErrMsg = 0;
	 int rc,i,SN=0;
	 sql *mysql;
	 char resultSNo[64][5];

	 mysql = SQL_Create();
	 rc = sqlite3_open("/web/cgi-bin/System.db", &mydb);
	 if( rc != SQLITE_OK ){
		      printf( "Lookup SQL error: %s\n", zErrMsg);
		      sqlite3_free(zErrMsg);
		   }
	 mysql->db = mydb;
	 mysql->tableName   =  "AlarmTestSegmentTable";	
         mysql->filedsName  =  "Status";
         mysql->filedsValue =  "-1";                                
         SN=SQL_findSNo(mysql,resultSNo);                          
         if(SN>0){
	     for(i =0 ;i<SN;i++)
	        {
		    printf("SNo:%s",resultSNo[i]);	                                  
                    mysql->filedsValue  =  "1";                                             
                    mysql->mainKeyValue =  resultSNo[i];                                    //需要修改状态的光路号
                    if(!semaphore_p())  
                        exit(EXIT_FAILURE);                                                 //P
                    rc=SQL_modify(mysql);
                    if( rc != SQLITE_OK ){
			 printf( "Modify SQL error\n");
			 sqlite3_free(zErrMsg);
		    }
                     if(!semaphore_v())                                                     //V
                         exit(EXIT_FAILURE);                 	 
	       }
         }

         mysql->filedsValue =  "-2";                                
         SN=SQL_findSNo(mysql,resultSNo);                          
         if(SN>0){
	     for(i =0 ;i<SN;i++)
	        {
		    printf("SNo:%s",resultSNo[i]);	                                  
                    mysql->filedsValue  =  "2";                                             
                    mysql->mainKeyValue =  resultSNo[i];                                    //需要修改状态的光路号
                    if(!semaphore_p())  
                        exit(EXIT_FAILURE);                                                 //P
                    rc=SQL_modify(mysql);
                    if( rc != SQLITE_OK ){
			 printf( "Modify SQL error\n");
			 sqlite3_free(zErrMsg);
		    }
                     if(!semaphore_v())                                                     //V
                         exit(EXIT_FAILURE);                 	 
	       }
         }

	 SQL_Destory(mysql);  
	 sqlite3_close(mydb);   
         return ;
} 

float realValue[8] ={4,4,4,4,4,4,4,4};                              //for test
alarmNode *  rollPolingAlarm(checkNode *headA,alarmNode *headB)
{
     	checkNode *p;
        alarmNode *q=NULL,*node;      
        int i=0;
        time_t nowTime;
	p= headA;
        q= headB;
	if(p==NULL){
		//printf("This is a void excel!\n");
		return q;
	}
	else
	  while(p!=NULL){
                if(realValue[i] < p->PowerGate ){    // 异常		   
                     nowTime = getLocalTimestamp(); 
                     if(p->fristAlarmFlag ==0){                                    //状态C: 首次出现异常 -->fristAlarmFlag=0   实际光功率值<阈值                 
                          printf("StateC--->SNo:%d -->i:%d realValue:%f <---> gateValue:%f\n",p->SNo,i,realValue[i],p->PowerGate);
		          p->fristAlarmFlag = 1;
			  node=(alarmNode *)malloc(sizeof(alarmNode));
		          node->SNo = p->SNo;
		          node->CM  = p->CM;
		          node->ANo = p->ANo;     
		          node->Order = (p->ANo)*100 +p->SNo; 
		          q=insert_B(q,node);  
	                  p->nextAlarmTime  = getLocalTimestamp()+p->alarmClick;
                     }else if(nowTime >= p->nextAlarmTime){                        //状态D:长期处于异常 -->fristAlarmFlag=1   实际光功率值<阈值  
                          printf("StateD--->SNo:%d -->i:%d realValue:%f <---> gateValue:%f\n",p->SNo,i,realValue[i],p->PowerGate);          
			  node=(alarmNode *)malloc(sizeof(alarmNode));
		          node->SNo = p->SNo;
		          node->CM  = p->CM;
		          node->ANo = p->ANo;     
		          node->Order = (p->ANo)*100 +p->SNo; 
		          q=insert_B(q,node);  
                          p->nextAlarmTime  = p->nextAlarmTime+p->alarmClick; 
                     }  
                 }                                 // 正常             
                 else if(p->fristAlarmFlag !=0){                                   //状态A:从异常中首次恢复 --> fristAlarmFlag=1 实际光功率值>=阈值      
                          printf("StateA--->SNo:%d -->i:%d realValue:%f <---> gateValue:%f\n",p->SNo,i,realValue[i],p->PowerGate);        
                          p->fristAlarmFlag = 0;
                      } 
                      else ;                                                       //状态B:正常  -->  fristAlarmFlag=0 实际光功率值>=阈值            
                i++; 
		p=p->next;                       
         }
       return q;
}
/***遍历链表***/
alarmNode * deleteALL_B(alarmNode *head){
	alarmNode *p;
	union sigval mysigval;
	char* process;  
	int ret = 0,n,signum;  
        pid_t cycPID[MAX_PID_NUM];  
	char * recvStr; 
	p= head;
	if(p==NULL){
		return p=NULL;
	}
	else{
	while(p!=NULL){
                   /*向otdrMain发送启动信号*/
                printf("Send message to otdrMain!\n");
		process ="/web/cgi-bin/otdrMain";                        
		ret = get_pid_by_name(process, cycPID, MAX_PID_NUM);  
		printf("process '%s' is existed? (%d): %c ", process, ret, (ret > 0)?'y':'n');  
		signum=SIGUSR1;                                         
		mysigval.sival_int = p->SNo+200;                                                      
		for(n=0;n<ret;n++){                                      
			printf("otdrMain PID:%u\n", cycPID[n]);                  
			if(sigqueue(cycPID[n],signum,mysigval)==-1)
				printf("send signal error\n");
			}  
                   /*等待信号的成功处理消息*/			    
		recvStr = (char *) malloc (sizeof (char)*10);
		recvStr = recvMessageQueue_B();
		if(strncmp(recvStr, "2-OK", 4) == 0)                    
			printf("Recv back message from otdrMain  sucessful!\n");
		else
			printf("Don't have any messges from otdrMain!\n");
		free(recvStr);
                //usleep(1);
		p=delete_B(p,p->SNo);
	}
         printf("\n\n");
        }    
       return p;
}


/***插入待启动节点***/
/*
      (1) 查询光路时间表状态
            --->BOA收到周期测试指令后，将待测试的光路状态修改为-1
            --->周期测试调度进程，查询数据库中状态为-1的节点. 
      (2) 加入测试节点
            --->将刚刚修改的光路加入周期测试链表
            --->BOA检查刚刚待测试状态为-1的光路，是否全部修改为1状态，若成功，则向中心服务器报告周期测试指令成功执行。
      (3) 检擦测试节点对应的光路数否已经存在
            --->若链表中SNo指定的光路存在，则只需更新测试参数（起始时间，下次启动时间、时间间隔）
            --->若不存在，则插入新的周期测试节点(先将其删除在插入链表。这样可保证更新的节点任然有序)
      (4)修改数据库时间表状态
            --->在数据库中，将刚刚检查出状态为-1的状态修改为1，保证所有待测试光路都处于测试状态
            --->修改完成后，即可向BOA发送测试节点加入成功消息
            --->修改过程中需要信号量进行数据库互斥访问(PV操作)
*/
checkNode *insertWaitingNode(checkNode *head)                    
{
	 sqlite3 *mydb;
	 char *zErrMsg = 0,*SNo;
	 int rc,i,SN=0;
	 sql *mysql;
	 char resultSNo[64][5];
         char **result = NULL;
         checkNode *node,*find;
         time_t T3,T4;
         head = link_creat_A();
         head = delete_A(head,0);                                     
         uint32_t ANo,protectFlag;
         float PowerGate;
         int    CM=0;
	 SNo = (char *) malloc(sizeof(char)*5);
	 mysql = SQL_Create();
	 rc = sqlite3_open("/web/cgi-bin/System.db", &mydb);
	 if( rc != SQLITE_OK ){
		      printf( "Lookup SQL error: %s\n", zErrMsg);
		      sqlite3_free(zErrMsg);
		   }
	 mysql->db = mydb;
	 mysql->tableName   =  "AlarmTestSegmentTable";	
         mysql->filedsValue =  "-1";                                
         mysql->filedsName  =  "Status";
         SN=SQL_findSNo(mysql,resultSNo);                           //查找光路状态为待启动的记录  光路状态为“-1”
         if(SN>0){
		for(i =0 ;i<SN;i++)
		    {
		        printf("SNo:%s",resultSNo[i]);
		        strcpy(SNo,resultSNo[i]);

		        mysql->filedsName    = "rtuCM"; 
		        mysql->mainKeyValue  = SNo;
		        rc= SQL_lookup(mysql,&result);
                        CM =atoi(result[0]);
		        printf("CM:%d\n",CM);

		        mysql->filedsName    = "Level"; 
		        mysql->mainKeyValue  = SNo;
		        rc= SQL_lookup(mysql,&result);
                        ANo =atoi(result[0]);
		        printf("ANo:%d\n",ANo);

		        mysql->filedsName    = "AT06"; 
                        mysql->mainKeyValue  =  SNo;
		        rc= SQL_lookup(mysql,&result);
                        PowerGate=atof(result[0]);
		        printf("PowerGate:%f\n",PowerGate);

		        mysql->filedsName    = "protectFlag"; 
		        rc= SQL_lookup(mysql,&result);
                        protectFlag=atoi(result[0]);
		        printf("protectFlag:%d\n",protectFlag);

		        mysql->filedsName    = "T3"; 
		        rc= SQL_lookup(mysql,&result);
                        T3 =computTime(result[0]);
		        printf("T3:%ld\n",T3);

		        mysql->filedsName    = "T4"; 
		        rc= SQL_lookup(mysql,&result);
                        T4= computTime(result[0]);
		        printf("T4:%ld\n",T4);

			node=(checkNode *)malloc(sizeof(checkNode));
			node->SNo           = atoi(SNo);
                        node->CM            = CM;
			node->ANo           = ANo;
			node->PowerGate     = PowerGate;
			node->protectFlag   = protectFlag;
			node->fristAlarmFlag= 0;
			node->nextAlarmTime = getLocalTimestamp();
			node->alarmClick    = T3;                                 
                        find=findNode_A(head,node->SNo);                          // 查看链表中是否已经存在SNo光路
                        if(find ==NULL)
                        {
                           head = insert_A(head,node);                
		        }else{
		           head = delete_A(head,node->SNo);         
                           head = insert_A(head,node); 
                        }

                       mysql->filedsValue  =  "1";                
                       mysql->filedsName   =  "Status";
                       mysql->mainKeyValue =   SNo;
                       if(!semaphore_p())  
                              exit(EXIT_FAILURE);                                 //P
                       rc=SQL_modify(mysql);
                       if( rc != SQLITE_OK ){
			      printf( "Modify SQL error\n");
			      sqlite3_free(zErrMsg);
		       }
                      if(!semaphore_v())                                          //V
                             exit(EXIT_FAILURE);
                         
		    }
         }
         free(SNo);
	 SQL_Destory(mysql);  
	 sqlite3_close(mydb); 

	if(result != NULL)
	{
	     if(result[0] != NULL)
		{
			free(result[0]);
			result[0] = NULL;
		}

		free(result);
		result = NULL;
	}  
         return(head);
}

/***删除待取消测试节点***/
/*
      (1) 查询障碍告警测试参数表状态
            --->BOA收到取消障碍告警测试参数指令后，将待取消测试的光路状态修改为-2
            --->障碍告警测试参数调度进程，查询数据库中状态为-2的节点.  
      (2) 删除待取消的测试节点
           --->先从链表中删除该SNo节点
           --->再更新光路状态为“2”(已取消)
*/
checkNode * removeWaitingNode(checkNode *head) 
{
	 sqlite3 *mydb;
	 char *zErrMsg = 0,*SNo;
	 int rc,i,SN=0,intSNo=0;
	 sql *mysql;
	 char resultSNo[64][5];
         checkNode *find;
	 SNo = (char *) malloc(sizeof(char)*5);
	 mysql = SQL_Create();
	 rc = sqlite3_open("/web/cgi-bin/System.db", &mydb);
	 if( rc != SQLITE_OK ){
		      printf( "Lookup SQL error: %s\n", zErrMsg);
		      sqlite3_free(zErrMsg);
		   }
	 mysql->db = mydb;
	 mysql->tableName   = "AlarmTestSegmentTable";	
         mysql->filedsValue =  "-2";                                
         mysql->filedsName  =  "Status";
         SN=SQL_findSNo(mysql,resultSNo);                           
         if(SN>0){
		 for(i =0 ;i<SN;i++)
		    {
		       printf("SNo:%s",resultSNo[i]);
		       strcpy(SNo,resultSNo[i]);		
		       intSNo =atoi(SNo);                                    
                       find=findNode_A(head,intSNo);                  
                        if(find ==NULL)
                        {
                          printf("Don't have SNo=%d node in this Link!\n",intSNo);                
		        }else{
                          head = delete_A(head,intSNo);                                             
                          mysql->filedsValue  =  "2";                                             
                          mysql->filedsName   =  "Status";
                          mysql->mainKeyValue =  SNo;
                          if(!semaphore_p())  
                              exit(EXIT_FAILURE);                                                 //P
                          rc=SQL_modify(mysql);
                          if( rc != SQLITE_OK ){
			      printf( "Modify SQL error\n");
			      sqlite3_free(zErrMsg);
		          }
                         if(!semaphore_v())                                                       //V
                             exit(EXIT_FAILURE);                  	
                        }
	            }
         }

         free(SNo);
	 SQL_Destory(mysql);  
	 sqlite3_close(mydb);   
         return(head);
}

void addNewtoLink(int signum,siginfo_t *info,void *myact);
void main(void)
{
        checkNode *node_A;
        alarmNode *node_B;

        /*初始化测试链表*/
        flushWaitingSNo();
        linkHead_check_A=InitA_CycleLink();
        linkHead_alarm_B=InitB_CycleLink();
        if(linkHead_check_A !=NULL)
            outPutALL_A(linkHead_check_A);
        else
            printf("linkA Head:NULL\n");

        if(linkHead_alarm_B !=NULL)
            outPutALL_B(linkHead_alarm_B);
        else
            printf("linkB Head:NULL\n");
        /*初始化信号机制（IPC）*/
        struct sigaction act;
        int sig;
        sig=SIGUSR1;  
        sigemptyset(&act.sa_mask);
        act.sa_sigaction=addNewtoLink;
        act.sa_flags=SA_SIGINFO|SA_RESTART;                                                                                                                                                               
        if(sigaction(sig,&act,NULL)<0){                              
          printf("install sigal error\n");
        }
        /*执行调度程序*/
        while(1){
             
            linkHead_alarm_B=rollPolingAlarm(linkHead_check_A,linkHead_alarm_B); 
            outPutALL_B(linkHead_alarm_B);
            linkHead_alarm_B=deleteALL_B(linkHead_alarm_B);
 
            sleep(1);
  
        }   	
}


void addNewtoLink(int signum,siginfo_t *info,void *myact)
{
       printf("the int value is %d \n",info->si_int);
       int SNo = info->si_int/100;
       int value =info->si_int %100;
       switch(info->si_int){
           case 130:{   
                    linkHead_check_A=insertWaitingNode(linkHead_check_A);        //启动告警测试
                    outPutALL_A(linkHead_check_A);
                    sendMessageQueue("130-OK");
		    break;
                  }
           case 230:{                                                            //终止告警测试
                    linkHead_check_A=removeWaitingNode(linkHead_check_A);                   
                    outPutALL_A(linkHead_check_A);
                    sendMessageQueue("230-OK");
		    break;
                  }

           case 170:{                                                            //设置光保护配对
                    sendMessageQueue("170-OK");
		    break;
                  }

           case 250:{                                                            //取消光保护配对
                    sendMessageQueue("250-OK");
		    break;
                  }

           case 370:{                                                            //请求光保护切换
                    sendMessageQueue("370-OK");
		    break;
                  }
          default:{                                                             //异步接收光功率异常(测试)
                    realValue[SNo]=(float)value;
                    break;
                  }
      }
}
