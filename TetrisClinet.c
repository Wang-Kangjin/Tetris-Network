/*
dztclinet.c
Author:Wang-Kangjin
E-mail:kangjin.wang@gmail.com
Data:Thursday 31 October 2013
Tetris clinet model
*/

#include <curses.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <signal.h>

#define PANELX 28
#define PANELY 15
#define PANELSTARTX 3
#define PANELSTARTY 2
#define PANELNEXTX 5
#define PANELNEXTY 37
#define SERVER_PORT 8888
#define BUFFERSIZE 420     //TCP发送数据时缓冲区的大小 28*15
#define CLIENTPOSX 3	//客户面板的长度
#define CLIENTPOSY 79  //客户面板的宽度

//存储一个块的形状位置等信息
typedef struct block {
  int group_id;
  int id;
  int pos_x;
  int pos_y;
  int color;
  int data[4][2];
}block;

void initblock(block*);
void init_background(int x,int y);
int init_network();				
void nextblock();				
void initpanel();
void printpanel(int, int);
void printclientpanel(WINDOW *,int (*arr)[PANELY]); //net work
void printnextpanel(int ,int);
void start();
void *  keylistener(void *arg);
void * server_send(void *arg);
void saveclient(int *);
void puttobuff(int *);
void goahead();

bool canmovedown();
bool canmoveleft();
bool canmoveright();
bool caninput();
bool canrotate();

void rotateleft();
void movedown();
void moveleft();
void moveright();
void movemid();
void nextblock();

void removeline(int);
void mergetotemp();
void savetoprev();
void eliminate();

void copy(block *, block *); //copy next block to current block
static void sig_handler(int);

int all_shape[28][4][2]={
  {{0,0},{1,0},{2,0},{3,0}},//0
  {{0,0},{0,1},{0,2},{0,3}},//0
  {{0,0},{1,0},{2,0},{3,0}},//0
  {{0,0},{0,1},{0,2},{0,3}},//0

  {{0,0},{1,0},{2,0},{2,1}},//0
  {{1,0},{1,1},{1,2},{0,2}},//0
  {{0,0},{0,1},{1,1},{2,1}},//00
  {{0,0},{0,1},{0,2},{1,0}},//

  {{0,1},{1,1},{2,0},{2,1}},//  0
  {{0,0},{0,1},{0,2},{1,2}},//  0
  {{0,0},{0,1},{1,0},{2,0}},// 00
  {{0,0},{1,0},{1,1},{1,2}},//

  {{0,0},{0,1},{1,1},{1,2}},//00
  {{0,1},{1,0},{1,1},{2,0}},// 00
  {{0,0},{0,1},{1,1},{1,2}},//
  {{0,1},{1,0},{1,1},{2,0}},//

  {{0,1},{0,2},{1,0},{1,1}},// 00
  {{0,0},{1,0},{1,1},{2,1}},//00
  {{0,1},{0,2},{1,0},{1,1}},//
  {{0,0},{1,0},{1,1},{2,1}},//

  {{0,0},{0,1},{1,0},{1,1}},//00
  {{0,0},{0,1},{1,0},{1,1}},//00
  {{0,0},{0,1},{1,0},{1,1}},
  {{0,0},{0,1},{1,0},{1,1}},

  {{0,1},{1,0},{1,1},{1,2}},// 0
  {{0,1},{1,0},{1,1},{2,1}},//000
  {{1,0},{1,1},{1,2},{2,1}},//
  {{0,1},{1,1},{1,2},{2,1}}
};

int temp_panel[PANELX][PANELY]; //用来存要显示的面板
int prev_panel[PANELX][PANELY]; //存上一次块落下后的现场，和现在正在下落的块左与运算，就得到要显示的样子，存放到temp_panel
int client_panel[PANELX][PANELY]; //显示客户端的样子

int score=0;				//分数

block block_current,block_next;

block *b1 = &block_current;		//指向当前下落的块
block *b2 = &block_next;		//指向下一个要下落的块

bool over=false;
int sleeptime=500000;
int level=0;

int main(void){
  start();
}


void start(){
  initscr();
  srand(time(0));
  init_background(0,0);			
  refresh();
  keypad(stdscr,TRUE);
  pthread_t keyboard;  					//键盘监听线程
  int err; 
  err = init_network();					//初始化网络模块
  if(err!=0){
    mvprintw(5,60,"Network model init failed!");
  }
 start:
  initpanel();						//把两个panel清空
  initblock(b1);initblock(b2);
  printpanel(PANELSTARTX,PANELSTARTY);
  err = pthread_create(&keyboard,NULL,keylistener,NULL);
  if(err !=0){
     printf("can't create thread keyboard listener!");
     exit(1);
    }

  //main thread make the block moving down until the gameover
  while(!over){
    movemid();
    if(caninput(b1)){			//可以放下当前块说明游戏未结束
      while( canmovedown() ){		//直到不能下落位置
	//继续下落
	goahead();	
	//显示一次			
	printpanel(PANELSTARTX,PANELSTARTY);

	usleep(sleeptime);
      }

      //save temp panel to preview panel and create new block
      savetoprev();
      printpanel(PANELSTARTX,PANELSTARTY);

	//停止下落后要消除
      eliminate();
	
	//把下一个块替换上来
      nextblock();
    }
    else
      over=true;
  }
  attrset(COLOR_PAIR(7));
  mvprintw(21,37,"YOU DEAD!Try again?(y/n):");
  int input;
  int quit = 1;
  while(quit){		//判断用户还要不要玩下去
    input=getch();
    switch(input){
    case 'y':  
      over = 0;
      attrset(COLOR_PAIR(0));
      mvprintw(21,37,"                         ");
      goto start;		//重新开始
      break;
    case 'n':  endwin();quit = 0;break;
    default: 
      mvprintw(21,40,"YOU DEAD!Try again?(y/n):");
  }
  }
}

void initpanel(){
  int i,j;
  for(i=0;i<PANELX;i++)
    for(j=0;j<PANELY;j++){
      temp_panel[i][j]=prev_panel[i][j]=0;
    }
}

void initblock(block *b){
  int line = rand()%28;
  int i ,j;
  //calculate the id in the group
  b->id=line%4;
  b->pos_x = 0;
  b->pos_y = 0;
  //calculate the groupid
  b->group_id = line/4;
  b->color = line/4+1;
  //init data area
  for(i=0;i<4;i++)
    for(j=0;j<2;j++)
      b->data[i][j]= all_shape[line][i][j];
}

void nextblock(){
  copy(b1,b2);
  initblock(b2);
}

//copy p_b2 to p_b1;
void copy(block *p_b1,block *p_b2){
  p_b1->group_id = p_b2->group_id;
  p_b1->id      = p_b2->id;
  p_b1->pos_x   = p_b2->pos_x;
  p_b1->pos_y   = p_b2->pos_y;
  p_b1->color   = p_b2->color;
  int i ,j;
  for(i=0;i<4;i++)
    for(j=0;j<2;j++)
      p_b1->data[i][j] =  p_b2->data[i][j];
}

void * keylistener(void *arg){
  noecho();
  int key;
  while(!over){
    key = getch();
    if(key == 'q' || key =='Q'){
      over = true;
    }

   switch(key){
    case KEY_UP:rotateleft();break;
    case KEY_LEFT:moveleft();break;
    case KEY_RIGHT:moveright();break;
    case KEY_DOWN:movedown();break;
    }
     printpanel(PANELSTARTX,PANELSTARTY);
  }
  
  pthread_exit((void *) 0);
}

//print panel at (x,y)
void printpanel(int x,int y){                        //打印当前panel状态（temp_panel）
  int i ,j ;
  int color;
  mergetotemp();
  for(i=0;i<PANELX;i++,x++){
       move(x,y);
    for(j=0;j<PANELY;j++){
      color = temp_panel[i][j];
      if(color != 0){
	attrset(COLOR_PAIR(color));
	printw("%s","[]");
      }
      else{
	attrset(COLOR_PAIR(0));
	printw("%s","  ");
      }
    }
  }
 
  printnextpanel(PANELNEXTX,PANELNEXTY);
  move(LINES-1,COLS-1);
  refresh();
}

 void printnextpanel(int x,int y){                      //显示下一个图形
   int arr_next[4][4];
   int i,j;
   int px=0,py=0;
   int color;
   for(i=0;i<4;i++)
     for(j=0;j<4;j++)
       arr_next[i][j]=0;
   for(i=0;i<4;i++){
     px=b2->data[i][0];
     py=b2->data[i][1];
     arr_next[px][py]=b2->color;
   }
   for(i=0;i<4;i++,x++){
     move(x,y);
     for(j=0;j<4;j++){
       color = arr_next[i][j];
       if(color !=0)
	 {
	   
	   attrset(COLOR_PAIR(color));
	   printw("%s","[]");
	 }
       else{
	 attrset(COLOR_PAIR(color));
	 printw("%s","  ");
       }
     }
   }
}

bool canmovedown(){
  if(over)
    return false;
  bool res=true;
  int i ;
  for(i=0;i<4;i++){
    int next_x=b1->data[i][0]+1;
    int next_y=b1->data[i][1];
    if(next_x>=PANELX)
      return false;
    res = res && (!prev_panel[next_x][next_y]);
    if(!res)
      return false;
  }
  return res;
}

bool caninput(block *p_b){
  int i ;
  for(i=0;i<4;i++){
    int prev_x=p_b->data[i][0] ;
    int prev_y=p_b->data[i][1] ;
    if(prev_panel[prev_x][prev_y] != 0 || prev_y<0 || prev_y>PANELY-1)
      return false;
  }
  return true;
}

void eliminate(){
                                  //检查有没有可消除的行。如果有就消除，并把上面的下移一行。
  int count=0;
  int i ,j;
  for(i=PANELX-1;i>=0;){            //遍历整个面板
    bool elim = true;
    for(j=0;j<PANELY;j++){
      if(prev_panel[i][j]==0)
	elim = false;    
    }
    if(elim){
      count++;                        //记录消除的行数
      for(j=0;j<PANELY;j++)
	prev_panel[i][j]=0;
      removeline(i);
    }
    else{
      i--;
    }
  }
  if(count != 0)
    score=score + (1<<count);
  if(level!=score/50+1){
    level = score/50+1;
    sleeptime= sleeptime- level*50000;
  }
  attrset(COLOR_PAIR(7));
  mvprintw(15,37," Your Score ");
  attrset(COLOR_PAIR(0));
  mvprintw(16,40,"%d",score);
  attrset(COLOR_PAIR(7));
  mvprintw(18,37," Your Level ");
  attrset(COLOR_PAIR(0));
  mvprintw(19,40,"%d",level);
}

void removeline(int i){                       //把上面一行挪下来就相当与消除，递归的进行，实现每一行都落下来
  int j;
  if(i == 0){
    for(j=0;j<PANELY;j++)
      prev_panel[i][j]=0;
    return ;
  }
  else{
    for(j=0;j<PANELY;j++)
      prev_panel[i][j]=prev_panel[i-1][j];
     removeline(i-1);
  }

}

void goahead(){                              //继续下落
  int i;
  for(i=0;i<4;i++)
    b1->data[i][0]++;
  b1->pos_x +=1  ;
}

void mergetotemp(){                                                //把下落的块和底部静态部分合起来，这样才能显示
  int i,j;
  for(i=0;i<PANELX;i++)
    for(j=0;j<PANELY;j++)
      temp_panel[i][j]= prev_panel[i][j];
  for(i=0;i<4;i++)
      temp_panel[ b1->data[i][0] ][ b1->data[i][1]] = b1->color;
}

void rotateleft(){
  if(canrotate()){
    b1->id = (b1->id+1)%4;
    int i ,j;
    int line = b1->group_id*4 + b1->id;
    for(i=0;i<4;i++){
	b1->data[i][0] = all_shape[line][i][0] + b1->pos_x;
	b1->data[i][1] = all_shape[line][i][1] + b1->pos_y;
    }
  }
}
bool canrotate(){
  struct block temp;
  struct block *p_temp=&temp;
  copy(p_temp,b1);                                         //先新建一个块尝试能不能转
  p_temp->id = (p_temp->id +1)%4;
  int i ,j;
  int line = p_temp->group_id*4 + p_temp->id;
  for(i=0;i<4;i++){
      p_temp->data[i][0] = all_shape[line][i][0] + b1->pos_x;
      p_temp->data[i][1] = all_shape[line][i][1] + b1->pos_y;
    }
  bool res = caninput(p_temp);
  return res;
}

void movedown(){
 while( canmovedown() ){
	goahead();	
      }
}

void moveleft(){
  int i;
  if(canmoveleft()){
    b1->pos_y--;
    for(i=0;i<4;i++)
      b1->data[i][1]--;
  }
}
bool canmoveleft(){
  int i ;
  for(i=0;i<4;i++){
    int x = b1->data[i][0];
    int y = b1->data[i][1];
    if( ((y-1) < 0) || ((prev_panel[x][y-1]) != 0) )
      return false;
  }
  return true;
}

void moveright(){
  int i;
  if(canmoveright()){
    b1->pos_y++;
    for(i=0;i<4;i++)
      b1->data[i][1]++;
  }
}
bool canmoveright(){
  int i ;
  for(i=0;i<4;i++){
    int x = b1->data[i][0];
    int y = b1->data[i][1];
    if( ((b1->data[i][1]+1) >= PANELY) || (prev_panel[x][y+1] != 0) )
      return false;
  }
  return true;
}


void movemid(){
  b1->pos_y=7;
  int i;
  for(i=0;i<4;i++)
    b1->data[i][1] +=7;
  
}
void savetoprev(){
  int i ,j;
  for(i=0;i<PANELX;i++)
    for(j=0;j<PANELY;j++)
      prev_panel[i][j] = temp_panel[i][j];    //保存现场。下一次显示为下面不动的部分
}

//画背景框
void init_background(int x,int y){
  int locate_Y_PLUS=y;
  int locate_X_PLUS=x;	
  int brush_x,brush_y;
  start_color()	;
  init_pair(0,COLOR_WHITE,COLOR_BLACK);
  init_pair(1,COLOR_BLACK,COLOR_RED);
  init_pair(2,COLOR_BLACK,COLOR_GREEN);
  init_pair(3,COLOR_BLACK,COLOR_YELLOW);
  init_pair(4,COLOR_BLACK,COLOR_BLUE);
  init_pair(5,COLOR_BLACK,COLOR_MAGENTA);
  init_pair(6,COLOR_BLACK,COLOR_CYAN);
  init_pair(7,COLOR_BLACK,COLOR_WHITE);

  for(brush_x=0+locate_X_PLUS;brush_x<=32+locate_X_PLUS;brush_x++)
    {
      for(brush_y=0+locate_Y_PLUS;brush_y<=110+locate_Y_PLUS;brush_y++)
	{
	  //"NEXT" box for player left
	  if(     ((brush_y==36+locate_Y_PLUS)&&(4+locate_X_PLUS<=brush_x)&&(brush_x<=9+locate_X_PLUS))
		  ||((brush_y==45+locate_Y_PLUS)&&(4+locate_X_PLUS<=brush_x)&&(brush_x<=9+locate_X_PLUS))
		  ||((brush_x==4+locate_X_PLUS)&&(36+locate_Y_PLUS<=brush_y)&&(brush_y<=45+locate_Y_PLUS))
		  ||((brush_x==9+locate_X_PLUS)&&(36+locate_Y_PLUS<=brush_y)&&(brush_y<=45+locate_Y_PLUS))
		  )
	    {
	      
	      attrset(COLOR_PAIR(7));	    
	      mvprintw(brush_x,brush_y," ");
	    }
	  //"NEXT" box for player right
	  if(     ((brush_y==65+locate_Y_PLUS)&&(4+locate_X_PLUS<=brush_x)&&(brush_x<=9+locate_X_PLUS))
		  ||((brush_y==74+locate_Y_PLUS)&&(4+locate_X_PLUS<=brush_x)&&(brush_x<=9+locate_X_PLUS))
		  ||((brush_x==4+locate_X_PLUS)&&(65+locate_Y_PLUS<=brush_y)&&(brush_y<=74+locate_Y_PLUS))
		  ||((brush_x==9+locate_X_PLUS)&&(65+locate_Y_PLUS<=brush_y)&&(brush_y<=74+locate_Y_PLUS))
		  )
	    {
	      attrset(COLOR_PAIR(7));	    
	      mvprintw(brush_x,brush_y," ");
	    }
	  //inner frame
	  if(   brush_y==32+locate_Y_PLUS
		||brush_y==78+locate_Y_PLUS
		||brush_y==1+locate_Y_PLUS
		||brush_y==109+locate_Y_PLUS
		||brush_x==1+locate_X_PLUS
		||brush_x==31+locate_X_PLUS
	       
		)
	    {
	      attrset(COLOR_PAIR(7));	    
	      mvprintw(brush_x,brush_y," ");
	    }	 
	  // outer frame
	  if(  brush_y==0+locate_Y_PLUS
	       ||brush_y==110+locate_Y_PLUS
	       ||brush_x==0+locate_X_PLUS
	       ||brush_x==32+locate_X_PLUS
	       )
	    {
	      attrset(COLOR_PAIR(1));	    
	      mvprintw(brush_x,brush_y," ");
	    }
	}
      printw("%s","\n");
    }/*for*/

  //information area
  move (4+locate_X_PLUS,48+locate_Y_PLUS);
  printw("%s"," <<NEXT CUBE>> ");
  move (14+locate_X_PLUS,47+locate_Y_PLUS);
  printw("%s"," <<<  SCORE  >>> ");
   attrset(COLOR_PAIR(4));
}
int init_network(){
  pthread_t serversend;
  pthread_t serverrecv;
  int res = pthread_create(&serversend,NULL,server_send,NULL);
  sleep(1);
  return res;
}
void * server_send(void *arg){
 //create a server socket
  WINDOW *server_window;   						//子窗口
  server_window = newwin(PANELX,2*PANELY,CLIENTPOSX,CLIENTPOSY);
  struct sockaddr_in client_addr;
  bzero(&client_addr,sizeof(client_addr));   //clear
  client_addr.sin_family = AF_INET;
  client_addr.sin_addr.s_addr = htons(INADDR_ANY);
  client_addr.sin_port = htons(0);
  attrset(COLOR_PAIR(7));
  
  int client_socket = socket(AF_INET,SOCK_STREAM , 0);
  if(client_socket < 0)
    mvwprintw(server_window,0,0,"CREATE SOCKET FAIL");
  if(bind(client_socket,(struct sockaddr*)&client_addr,sizeof(client_addr)))
     mvwprintw(server_window,0,0,"BIND SOCKET FAIL");

  struct sockaddr_in server_addr;
  bzero(&server_addr,sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  if(inet_aton("192.168.102.73",&server_addr.sin_addr)==0)                     //ip需要修改
           mvwprintw(server_window,0,0,"inet_aton FAIL");
  server_addr.sin_port = htons(SERVER_PORT);
  socklen_t length = sizeof(server_addr);
  mvwprintw(server_window,0,0,"CONNECING... ...");
  touchwin(server_window);
  wrefresh(server_window);
  
  if( connect(client_socket,(struct sockaddr*)&server_addr,length) == 0)      //请求连接
    {
      mvwprintw(server_window,1,0,"CONNECT SUCCESS");
      sleep(2);
    }
  else{
    mvwprintw(server_window,1,0,"CONNECT FIALED!");
    wrefresh(server_window);
    pthread_exit((void *) 0);
  }
  touchwin(server_window);wrefresh(server_window);
   int buffer[BUFFERSIZE];
   signal(SIGPIPE,sig_handler);
  while(1){
    bzero(buffer,BUFFERSIZE);
       puttobuff(buffer);                   //先放到buffer里再发送
 
       //   mvwprintw(server_window,2,0,"Sending ... ...");
    length = send(client_socket,buffer,sizeof(buffer),0);  
    if(length < 0)
      {
	
	break;
      } 
 
    length = recv(client_socket,buffer,sizeof(buffer),0);
    if(length == 0){
      break;
    }
    if(length <0)
      {
   	mvwprintw(server_window,LINES-1,0,"recv error!");
   	break;
     }
    saveclient(buffer);            //把buffer转化为二维数组，放到clinet_panel里
    
    printclientpanel(server_window,client_panel);
     usleep(500000);
  }
  close(client_socket);
  mvwprintw(server_window,0,0,"He is DEAD,you WIN !!!");
  wrefresh(server_window);
  
  delwin(server_window);
  pthread_exit((void *) 0);
}

void saveclient(int *buff){
  int i,j;
  int count = 0;
  for(i=0;i<PANELX;i++)
    for(j=0;j<PANELY;j++,count++)
      {
	client_panel[i][j] = buff[count];
      }
}
void puttobuff(int *buff){          
  int i,j;
  int count=0;
  for(i=0;i<PANELX;i++)
    for(j=0;j<PANELY;j++,count++){
      buff[count]=temp_panel[i][j];
    }
}
void printclientpanel(WINDOW *win,int (*arr)[PANELY]){
  int i ,j ;
  int x=0,y=0;
  int color;
  for(i=0;i<PANELX;i++,x++){
       move(x,y);
       for(j=0,y=0;j<PANELY;j++,y+=2){
      color = arr[i][j];
      if(color != 0){
	attrset(COLOR_PAIR(color));
	mvwprintw(win,x,y,"%s","[]");
      }
      else{
	attrset(COLOR_PAIR(0));
	mvwprintw(win,x,y,"%s","  ");
      }
    }
  }
  wrefresh(win);
}
static void sig_handler(int sig){
  if(sig == SIGPIPE)
    {
      
    }
}
