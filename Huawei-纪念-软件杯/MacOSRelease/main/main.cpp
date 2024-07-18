#include <bits/stdc++.h>
using namespace std;

//ofstream file("debug.txt");

const int n = 200;
const int robot_num = 10;
const int berth_num = 10;
const int active_berth_num=10;      //初始激活的港口数量
const int target_val=20;            //短距离优先寻找的货物最小价值   重要的调试价值
const int pong_check=3;              //碰撞检测
const int start_ship=2;            //启动船只前往港口的阀值  调小记得还有伯口尽量满船出发
const int disable_step_berth=30;    //关闭港口的最大距离，距离之内的港口要关闭  重要的调试价值
const int change_limit=10;   //用于给当前港口加权，以免每次update最新港口时，造成的随意晃动
const int punish_robot=3;   //根据机器人数量调整港口权重
const int save_steps=0;
const int N = 210;
int money, boat_capacity;
int frame_time;
int fast_berth_id;
char ch[N][N];
char gds[N][N];
int maps[berth_num][N][N];



vector<int> robot_status;
vector<int> berth_tran(berth_num,1);
vector<int> berth_status(berth_num,-1);//-1 没有船  0～9 有船前往     10～15 船已经抵达  编号为0-5
vector<int> berth_sum(berth_num,0); // 统计每个港口的累积的货物数量
vector<int> berth_priority(berth_num,0); //设置港口优先级  0 港口关闭 1 港口没有船且累积数量达到boat_capacity 2  港口有船且船的货物数量加上巷口累积数量达到boat_capacity 3 港口没有船且累积数量不足boat_capacity 4 港口有船且船的货物数量加上港口累积的数量不足boat_capacity
vector<int> goods_nums(berth_num,0);
vector<string> robot_action;
vector<string> boat_action;
vector<pair<int,int> > trans_time;


struct Goods;
struct Store;
struct Berth;
struct Robot;
struct Boat;
struct Step;

vector<set<pair<int,Goods*> > >  virtual_heads(berth_num,set<pair<int,Goods*> >());
unordered_map<int,Goods *> quick_map;



int reverseDir(int direction);
int findBerth(int rid);
void tryfindBerth(int rid);
int findBerthShort(int x,int y);
void undate_berth_priority();

// void print_berth(){
//     for(int i=0;i<berth_num;i++)

// }

vector<int> searchPath(int x,int y);
struct Step{
    int x,y;
    int steps_count;
    Step(int xx,int yy,int sc){
        x=xx;
        y=yy;
        steps_count=sc;
    }
};


struct Goods{
    int x,y;
    int val;
    int frame_id;
    int berth_id;
    int status;  //0 初始 1 有机器人前往  2 有机器人拿到
    Goods * front;
    Goods * next;
    Goods(){
        front=next=nullptr;
        status=0;
    }
    Goods(int startX, int startY,int init_val,int create_frame_id){
        x=startX;
        y=startY;
        val=init_val;
        status=0;
        frame_id=create_frame_id;
        front=next=nullptr;
        berth_id=findBerthShort(x,y);
        goods_nums[berth_id]++;
    }
};


struct  Store
{
    Goods* head;
    Store(){head = new Goods();}
    void addGoods(Goods * g){
        if(g==nullptr)  
            return;
        quick_map[g->x*500+g->y]=g;
        Goods * p=head->next;
        Goods * last=head;
        if(p==nullptr){
            head->next=g;
            g->front=head;
            return;
        }
        while(p!=nullptr){
            if(g->val>=p->val){
                last->next=g;
                g->front=last;
                g->next=p;
                p->front=g;
                return;
            }
            last=p;
            p=p->next;  
        }
        last->next=g;
        g->front=last;
        
    }
    Goods * findReverseGoods(int x,int y,int oldx,int oldy){ 
        Goods * p=head->next;
        if(p==nullptr)
            return nullptr;
        int tmpx=(oldx)*(p->x-x);
        int tmpy=(oldy)*(p->y-y);
        while(!(tmpx<=0&&tmpy<=0))
        {
            p=p->next;
            if(p==nullptr)
                break;
            tmpx=(oldx)*(p->x-x);
            tmpy=(oldy)*(p->y-y);
        }
        return p;
    }
    Goods * byDistance(int rid);
    Goods * byValue(int rid);
    Goods * findGoods(int rid);
    Goods * byFrameID(int rid);
    Goods * nextGoods(){ //可能有更精细的规划
        Goods * p=head->next;
        while(p!=nullptr){
            if(p->status==0){
                break;
            }
            p=p->next;
        }
        return p;
    }

    void deleteGoods(Goods * cur){
        if(cur==nullptr)
            return;
        quick_map[cur->x*500+cur->y]=nullptr;
        Goods * p=cur->front;
        p->next=cur->next;
        if(cur->next!=nullptr)
            cur->next->front=p;
        delete cur;
    }
}store;


struct Berth
{
    int x;
    int y;
    int ori_x;
    int ori_y;
    int transport_time;
    int loading_speed;
    Berth(){}
    Berth(int x, int y, int transport_time, int loading_speed) {
        this -> x = x;
        this -> y = y;
        this -> transport_time = transport_time;
        this -> loading_speed = loading_speed;
    }
    bool check(int tx,int ty){
        return (tx>=x&&tx<=tx+3)&&(ty>=y&&ty<=y+3);
    }
}berth[berth_num + 10];

struct Robot
{
    int x, y, goods;
    int status;
   // int mbx,mby;
    //new
    Goods * gr;
    int berth_id;
    int last_direction;
    int pointer;
    int dump;
    vector<int> path;
    int restore_tag;//0 未处于恢复模式 1 准备中 2 进入恢复模式，被往哪推就往哪走  3 开始恢复，回溯原来的路径
    stack<int> restore_path;
    Robot() {}
    Robot(int startX, int startY) {
        x = startX;
        y = startY;
        gr=nullptr;
        status=0;
        dump=0;
        berth_id=-1;
        pointer=0;
        restore_tag=0;
        last_direction=-1;
    }
    //new
    void init_goods(Goods * g){//可能为nulptr
        last_direction=-1;
        gr=g;
        pointer=0;
        dump=0;
        path.clear();
        restore_path=stack<int>();
        restore_tag=0;
        if(g!=nullptr)
        {
            g->status=1;
            berth_id=g->berth_id;
            vector<Step> goods_to_berth;  //这时候表示，从x,y出发，sc表示前进的方向
            unordered_map<int,int> fast_search;
            int tx=g->x;
            int ty=g->y;
            int tmp;
            while(tx!=berth[berth_id].x||ty!=berth[berth_id].y){
                tmp=maps[berth_id][tx][ty]-1;
                fast_search[tx*500+ty]=goods_to_berth.size();
                if(tmp==maps[berth_id][tx+1][ty]){
                    goods_to_berth.emplace_back(tx,ty,3);
                    tx++;
                }
                else if(tmp==maps[berth_id][tx-1][ty]){
                    goods_to_berth.emplace_back(tx,ty,2);
                    tx--;
                }
                else if(tmp==maps[berth_id][tx][ty+1]){
                    goods_to_berth.emplace_back(tx,ty,0);
                    ty++;
                }
                else if(tmp==maps[berth_id][tx][ty-1]){
                    goods_to_berth.emplace_back(tx,ty,1);
                    ty--;
                }
            }
            fast_search[(berth[berth_id].x)*500+berth[berth_id].y]=goods_to_berth.size();
            goods_to_berth.emplace_back(berth[berth_id].x,berth[berth_id].y,-1);
            tx=x;
            ty=y;
            //从机器人开始找
            
            queue<Step> q;
            vector<vector<int> >map(N,vector<int>(N,0));
            q.push(Step(x,y,1));
            map[x][y]=1;
           
            while(q.size()){
                Step tmp=q.front();
                q.pop();
                tx=tmp.x;
                ty=tmp.y;
                if(fast_search.count(tx*500+ty)){
                    break;
                }
                tmp.steps_count++;
                if(map[tmp.x+1][tmp.y]==0&&((ch[tmp.x+1][tmp.y]=='.')||(ch[tmp.x+1][tmp.y]=='B'))){
                    map[tmp.x+1][tmp.y]=tmp.steps_count;
                    q.push(Step(tmp.x+1,tmp.y,tmp.steps_count));
                }
                if(map[tmp.x-1][tmp.y]==0&&((ch[tmp.x-1][tmp.y]=='.')||(ch[tmp.x-1][tmp.y]=='B'))){
                    map[tmp.x-1][tmp.y]=tmp.steps_count;
                    q.push(Step(tmp.x-1,tmp.y,tmp.steps_count));
                }
                if(map[tmp.x][tmp.y+1]==0&&((ch[tmp.x][tmp.y+1]=='.')||(ch[tmp.x][tmp.y+1]=='B'))){
                    map[tmp.x][tmp.y+1]=tmp.steps_count;
                    q.push(Step(tmp.x,tmp.y+1,tmp.steps_count));
                }
                if(map[tmp.x][tmp.y-1]==0&&((ch[tmp.x][tmp.y-1]=='.')||(ch[tmp.x][tmp.y-1]=='B'))){
                    map[tmp.x][tmp.y-1]=tmp.steps_count;
                    q.push(Step(tmp.x,tmp.y-1,tmp.steps_count));
                }
                
            }
            if(fast_search.count(tx*500+ty)==0)
            {
                g->status=0;
                berth_id=findBerthShort(x,y);
                gr=nullptr;
                return;
            } 
            
            
            int idx=fast_search[tx*500+ty]-1; 
            vector<int>tmp_path;
            while(map[tx][ty]!=1){
                tmp=map[tx][ty]-1;
                
                if(tmp==map[tx+1][ty]){
                    tmp_path.push_back(3);
                    tx++;
                }
                else if(tmp==map[tx-1][ty]){
                    tmp_path.push_back(2);
                    tx--;
                }
                else if(tmp==map[tx][ty+1]){
                    tmp_path.push_back(0);
                    ty++;
                }
                else if(tmp==map[tx][ty-1]){
                    tmp_path.push_back(1);
                    ty--;
                }

            }
            for(int i=tmp_path.size()-1;i>=0;i--)
                path.push_back(reverseDir(tmp_path[i]));
            while(idx>=0){
                path.push_back(reverseDir(goods_to_berth[idx].steps_count));
                idx--;
            }
            if(path.size()+frame_time >= 1000+ g->frame_id)
            {
                g->status=0;
                berth_id=findBerthShort(x,y);
                gr=nullptr;
            }
        }
        
    }
    
    void try_switch_goods(int force){
        vector<vector<int> > map(N,vector<int>(N,0));
        map[x][y]=1;
        queue<Step> q;
        q.push(Step(x,y,1));
        int tx,ty;
        int len=path.size()-pointer;
        if(force||len==0)
            len=0x7fffffff;
        while(q.size()){
            Step tmp=q.front();
            q.pop();
            tx=tmp.x;
            ty=tmp.y;
            if(quick_map[tx*500+ty]!=nullptr&&quick_map[tx*500+ty]->status==0&&quick_map[tx*500+ty]->val>target_val){
                break;
            }
            if(tmp.steps_count>=len){
                break;
            }
            tmp.steps_count++;
            
            if(map[tmp.x+1][tmp.y]==0&&((ch[tmp.x+1][tmp.y]=='.')||(ch[tmp.x+1][tmp.y]=='B'))){
                map[tmp.x+1][tmp.y]=tmp.steps_count;
                q.push(Step(tmp.x+1,tmp.y,tmp.steps_count));
            }
            if(map[tmp.x-1][tmp.y]==0&&((ch[tmp.x-1][tmp.y]=='.')||(ch[tmp.x-1][tmp.y]=='B'))){
                map[tmp.x-1][tmp.y]=tmp.steps_count;
                q.push(Step(tmp.x-1,tmp.y,tmp.steps_count));
            }
            if(map[tmp.x][tmp.y+1]==0&&((ch[tmp.x][tmp.y+1]=='.')||(ch[tmp.x][tmp.y+1]=='B'))){
                map[tmp.x][tmp.y+1]=tmp.steps_count;
                q.push(Step(tmp.x,tmp.y+1,tmp.steps_count));
            }
            if(map[tmp.x][tmp.y-1]==0&&((ch[tmp.x][tmp.y-1]=='.')||(ch[tmp.x][tmp.y-1]=='B'))){
                map[tmp.x][tmp.y-1]=tmp.steps_count;
                q.push(Step(tmp.x,tmp.y-1,tmp.steps_count));
            }
            
        }

        if(quick_map[tx*500+ty]!=nullptr&&quick_map[tx*500+ty]->val>target_val){
            last_direction=-1;
            Goods * g=quick_map[tx*500+ty];
            gr=g;
            pointer=0;
            dump=0;
            path.clear();
            restore_path=stack<int>();
            restore_tag=0;
            g->status=1;
            berth_id=g->berth_id;
            int tmp;
            vector<int> tmp_path;
            while(map[tx][ty]!=1){
                tmp=map[tx][ty]-1;
                
                if(tmp==map[tx+1][ty]){
                    tmp_path.push_back(3);
                    tx++;
                }
                else if(tmp==map[tx-1][ty]){
                    tmp_path.push_back(2);
                    tx--;
                }
                else if(tmp==map[tx][ty+1]){
                    tmp_path.push_back(0);
                    ty++;
                }
                else if(tmp==map[tx][ty-1]){
                    tmp_path.push_back(1);
                    ty--;
                }

            }
            for(int i=tmp_path.size()-1;i>=0;i--)
                path.push_back(reverseDir(tmp_path[i]));
        }
    

    }

    vector<int> goBerth(){
        int tmp;
        //if(berth_id!=-1)
            tmp=maps[berth_id][x][y]-1;
        // else    
        //     tmp=0;
        vector<int>idir;
        if(tmp==0)
        {
            idir.push_back(-1);
            return idir;
        }
        if(tmp==maps[berth_id][x+1][y]){
            idir.push_back(3);
        }
        if(tmp==maps[berth_id][x-1][y]){
            idir.push_back(2);
        }
        if(tmp==maps[berth_id][x][y+1]){
            idir.push_back(0);
        }
        if(tmp==maps[berth_id][x][y-1]){
            idir.push_back(1);
        }
        if(idir.empty()){ //基本上全图覆盖不存在这样的情况，除非是封闭的，那就没意义了
            idir.push_back(-1);
        }
        return idir;
    }
    
    vector<int> goGoods(){
        vector<int>idir;
        if(restore_tag>=2){
            restore_tag=3;
            // 
            idir.push_back(restore_path.top());
            return idir;
        }
        
        if(gr==nullptr||path.size()==pointer){
            idir.push_back(-1);
            return idir;
        }
        idir.push_back(path[pointer]);
        return idir;
    }
   
    

    // return the intentional direction
    vector<int> preRun(){  //其他方向 或 停止 -1 
        if(goods)
            return goBerth();
        else    
        {
            if(gr==nullptr)
                return searchPath(x,y);
            vector<int> idir= goGoods();
            if(idir[0]==-1)
                return idir;
            int pre_dir=path[pointer];
            //说明这是违背祖宗的决定
            if(restore_tag==0&&reverseDir(pre_dir)==last_direction){ // last_dir被修改过， 发生碰撞的处理, 希望回退一格
                // idir.clear();     
                // if(pointer>0)
                //     idir.push_back(reverseDir(path[pointer-1]));
                // else//已经回到原点，无法违背祖宗，进入恢复模式
                // {   
                    restore_tag=1;
                    idir=searchPath(x,y);
                    //idir.push_back(-1);
               // }
            }
            return idir;
        }
    }

}robot[robot_num + 10];


struct Boat
{
    int num, pos, status;
}boat[10];





void InitMap(){
    for(int i=0;i<berth_num;i++){
        queue<Step> q;
        q.push(Step(berth[i].x,berth[i].y,1));
        maps[i][berth[i].x][berth[i].y]=1;
        while(q.size()){
            Step tmp=q.front();
            q.pop();
            tmp.steps_count++;
            if(maps[i][tmp.x+1][tmp.y]==0&&((ch[tmp.x+1][tmp.y]=='.')||(ch[tmp.x+1][tmp.y]=='B'))){
                maps[i][tmp.x+1][tmp.y]=tmp.steps_count;
                q.push(Step(tmp.x+1,tmp.y,tmp.steps_count));
            }
            if(maps[i][tmp.x-1][tmp.y]==0&&((ch[tmp.x-1][tmp.y]=='.')||(ch[tmp.x-1][tmp.y]=='B'))){
                maps[i][tmp.x-1][tmp.y]=tmp.steps_count;
                q.push(Step(tmp.x-1,tmp.y,tmp.steps_count));
            }
            if(maps[i][tmp.x][tmp.y+1]==0&&((ch[tmp.x][tmp.y+1]=='.')||(ch[tmp.x][tmp.y+1]=='B'))){
                maps[i][tmp.x][tmp.y+1]=tmp.steps_count;
                q.push(Step(tmp.x,tmp.y+1,tmp.steps_count));
            }
            if(maps[i][tmp.x][tmp.y-1]==0&&((ch[tmp.x][tmp.y-1]=='.')||(ch[tmp.x][tmp.y-1]=='B'))){
                maps[i][tmp.x][tmp.y-1]=tmp.steps_count;
                q.push(Step(tmp.x,tmp.y-1,tmp.steps_count));
            }
        }
        
    }
}
void Init()
{

    for(int i = 1; i <= n; i ++)
    {    
        scanf("%s", ch[i] + 1);
        for(int j=1;ch[i][j]!=0;j++)
            if(ch[i][j]=='A')
            {
                ch[i][j]='.';
            }
    }
    
    int tmp_trann_time=0x7fffffff;
    for(int i = 0; i < berth_num; i ++)
    {
        int id;
        scanf("%d", &id);
        scanf("%d%d%d%d", &berth[id].x, &berth[id].y, &berth[id].transport_time, &berth[id].loading_speed);
        berth[id].x++;
        berth[id].y++;
        berth[id].ori_x=berth[id].x;
        berth[id].ori_y=berth[id].y;
        // file << "berth id "<<id<<"  tranport time "<<berth[id].transport_time<<endl;
        if(berth[id].transport_time<tmp_trann_time)
        {
            tmp_trann_time=berth[id].transport_time;
            fast_berth_id=id;
        }
        if(ch[berth[id].x-1][berth[id].y-1]=='*')
            continue;
        else if(ch[berth[id].x-1][berth[id].y+4]=='*'){ //关键点
            berth[id].y+=3;
        }
        else if(ch[berth[id].x+4][berth[id].y-1]=='*'){ //关键点
            berth[id].x+=3;
        }
        else if(ch[berth[id].x+4][berth[id].y+4]=='*'){ //关键点
            berth[id].x+=3;
            berth[id].y+=3;
        }
    }

    for(int i=0;i<5;i++)
        boat[i].num=0,boat[i].pos=-1,boat[i].status=1;

    scanf("%d", &boat_capacity);
    
    for(int i=0;i<berth_num;i++)
        trans_time.emplace_back(2*berth[i].transport_time+boat_capacity/berth[i].loading_speed,i);
    sort(trans_time.begin(),trans_time.end());
    for(int i=0;i<active_berth_num;i++)
        berth_priority[trans_time[i].second]=3;
    
    char okk[100];
    scanf("%s", okk);
    InitMap();

    vector<int> relation(berth_num,0);
    for(int i=0;i<berth_num;i++){
        for(int j=0;j<berth_num;j++){
            if(maps[i][berth[j].x][berth[j].y]!=0)
                relation[i]+=(1<<j);
        }
    }
    int disable_num=0;
    for(int i=0;i<berth_num;i++){
        int bid=trans_time[i].second;
        if(berth_priority[bid]==0)
            continue;
        for(int j=0;j<berth_num;j++){
            if(j!=bid&&berth_priority[j]!=0&&relation[bid]==relation[j]){ //两个港口功能冲突
                if(maps[bid][berth[j].x][berth[j].y]<disable_step_berth)//他们距离太近了
                {
                    berth_priority[j]=0; //禁用该港口
                    disable_num++;
                }
            }
        }
    }
    // if(dis)

    printf("OK\n");
    fflush(stdout);
}

void Input()
{
    robot_status=vector<int>(robot_num,0);
    robot_action.clear();
    boat_action.clear();
    int id;
    scanf("%d%d", &id, &money);
    frame_time=id;
    int num;
    scanf("%d", &num);
    for(int i = 1; i <= num; i ++)
    {
        int x, y, val;
        scanf("%d%d%d", &x, &y, &val);
        x++;
        y++;
        if(ch[x][y]=='.'&&findBerthShort(x,y)!=-1){
            store.addGoods(new Goods(x,y,val,id));
            if(gds[x][y]==0)
                gds[x][y]='G';
        }else{
            //对港口的数量影响不大 todo 
        }
    }
    for(int i = 0; i < robot_num; i++)
    {
        int sts;
        int x=robot[i].x;
        int y= robot[i].y;
        //gds[x][y]=0;  仅仅为了应付碰撞
        //需要检测，预测的机器人位置和判题器给的是否一致  通过
        scanf("%d%d%d%d", &robot[i].goods, &robot[i].x, &robot[i].y, &sts);
        robot[i].x++;
        robot[i].y++;
        if((x!=robot[i].x||y!=robot[i].y)&&x!=0&&y!=0){  //当预测的位置和实际位置不对，避免碰撞，更新机器人状态
            // file<<"Error inconsistent"<<endl;
            gds[x][y]=0;
            if(robot[i].goods==0&&robot[i].gr!=nullptr){
                robot[i].gr->status=0;
                robot[i].init_goods(store.findGoods(i));
            }
        }
        //gds[robot[i].x][robot[i].y]='a'+i;
        if(gds[robot[i].x][robot[i].y]<'a')
        {    
            gds[robot[i].x][robot[i].y]='a'+i;  
            robot[i].berth_id=findBerthShort(robot[i].x,robot[i].y);
        }
        // if(robot[i].berth_id==-1)
        // {
        //     if(robot[i].goods)
        //         robot[i].berth_id=findBerth(i);
        //     else
        //         robot[i].berth_id=findBerthShort(robot[i].x,robot[i].y);
        // }
         //robot[i].berth_id=findBerthShort(robot[i].x,robot[i].y);
        
        
            

        
        

       
    }
    for(int i = 0; i < 5; i ++){
        scanf("%d%d\n", &boat[i].status, &boat[i].pos);
    }
    char okk[100];
    scanf("%s", okk);
}

//Store 函数
Goods * Store::byDistance(int rid){
    int bid;
    if(robot[rid].status)
    {
        bid=robot[rid].status-10;
        robot[rid].status=0;
    }else{
        bid=findBerthShort(robot[rid].x,robot[rid].y);
    }
    if(bid==-1)
        return nullptr;
    int nums;
    if(berth_status[bid]>=0){
        for(int i=0;i<5;i++){
            if(boat[i].pos==bid)   
                nums+=(boat_capacity-boat[i].num);
        }
        nums-=berth_sum[bid];
    }else{
        nums=boat_capacity-berth_sum[bid];
    } 
    nums/=3;
    if(!nums)
        nums=1;
    nums=min(15,nums);
    int ma=-1;
    int mi=0x7fffffff;
    Goods * ans=nullptr;
    int tmp_val=-1;
    int tmp_steps=-1;
    //new
    for(auto & it:virtual_heads[bid]){
        // if(it.first==tmp_steps){
        //     if(it.second->val>tmp_val){
        //             mi=it.second->frame_id+1000;
        //             ans=it.second;
        //             tmp_steps=it.first;
        //             tmp_val=it.second->val;
        //     }
        // }
        if(!nums)
            break;
        if(it.second->status==0){
            if(it.second->val>target_val){
                nums--;
                int end_time=it.second->frame_id+1000;
                if(end_time<mi&&frame_time+maps[bid][it.second->x][it.second->y]<=end_time){
                    mi=end_time;
                    ans=it.second;
                    tmp_steps=it.first;
                    tmp_val=it.second->val;
                }
            }else if(ans==nullptr){
                ans=it.second;
            }
        }

    }
    //origin
    
    
    // for(auto & it:virtual_heads[bid]){
        
    //     if(it.second->status==0)
    //     {
    //         if(tmp_val!=-1){
    //             if(tmp_steps==it.first)
    //             {
    //                 if(it.second->val>tmp_val)
    //                 {
    //                     ans=it.second;
    //                     tmp_val=it.second->val;
    //                 }
    //             }else{
    //                 break;
    //             }
    //         }
    //         if(it.second->val>target_val){            
    //             ans = it.second;
    //             tmp_val=it.second->val;
    //             tmp_steps=it.first;
    //         }
    //         else{
    //             if(it.second->val>ma)
    //             {
    //                 ans=it.second;
    //                 ma=it.second->val;
    //             }
    //         }
    //     }
    // }
    
    
    return ans;
    // int x=robot[rid].x;
    // int y=robot[rid].y;
    // Goods *p =head->next;
    // double dis=0x7fffffff;
    // Goods * ans=nullptr;
    // int tmp;
    // int bid=findBerthShort(x,y);
    // //int bid=robot[rid].berth_id;
    // int dis2=0x7fffffff;
    // Goods * ans2=nullptr;
    // while(p!=nullptr){
    //     if(p->status==0){
    //         tmp=maps[bid][p->x][p->y];
    //         //bid==p->berth_id
    //         if(tmp<dis&&bid==p->berth_id){
    //             if(p->val>target_val)
    //             {
    //                 dis=tmp;
    //                 ans=p;
    //             }else if (ans==nullptr&&tmp<dis2&&maps[bid][x][y]!=0){
    //                 dis2=tmp;
    //                 ans2=p;
    //             }
    //         }
    //         if(ans==nullptr&&tmp<dis2&&maps[p->berth_id][x][y]!=0){
    //             dis2=tmp;
    //             ans2=p;
    //         }
    //     }
    //     p=p->next;
    // }
    // ans=ans==nullptr?ans2:ans;
    // return ans;
}
Goods * Store::byValue(int rid){
     int x=robot[rid].x;
    int y=robot[rid].y;
    Goods *p =head->next;
    Goods * ans=nullptr;
    int val2=-1;
    Goods * ans2=nullptr;
    int bid=findBerthShort(x,y);
    //int bid=robot[rid].berth_id;
    while(p!=nullptr){
        if(p->status==0&&maps[bid][p->x][p->y]!=0){
            //p->berth_id!=bid
            if(p->berth_id==bid){
                ans=p;
                break; //本身货物链表就是按照价值递减的顺序排列
            }
            if(ans==nullptr&&p->val>val2){
                val2=p->val;
                ans2=p;
            }
        }
        p=p->next;
    }
    ans=ans==nullptr?ans2:ans;
    return ans;
}
Goods * Store::byFrameID(int rid){
    int x=robot[rid].x;
    int y=robot[rid].y;
    Goods *p =head->next;
    double mi=0x7fffffff;
    Goods * ans=nullptr;
    int bid=findBerthShort(x,y);
    //int bid=robot[rid].berth_id;
    int dis2=0x7fffffff;
    Goods * ans2=nullptr;
    while(p!=nullptr){
        if(p->status==0&&maps[p->berth_id][x][y]!=0){
            if(bid==p->berth_id&&p->frame_id<mi&&p->frame_id+900>=frame_time){
                if(p->val>target_val)
                {
                    mi=p->frame_id;
                    ans=p;

                }else if (ans==nullptr&&p->frame_id<dis2){
                    dis2=p->frame_id;
                    ans2=p;
                }
            }
            if(ans==nullptr&&p->frame_id<dis2){
                dis2=p->frame_id;
                ans2=p;
            }
        }
        p=p->next;
    }
    ans=ans==nullptr?ans2:ans;
    return ans;
}
Goods * Store::findGoods(int rid){
    if(berth_priority[robot[rid].berth_id]==1||berth_priority[robot[rid].berth_id]==2)//港口无船
    // if(berth_status[robot[rid].berth_id]<0)    
       return byValue(rid);
    // else if(berth_status[robot[rid].berth_id]<10)
    //     return byFrameID(rid);
        else
        return byDistance(rid);
}
//

//查找最近港口
int findBerthShort(int x,int y){
        int mi=0x7fffffff;
        int select_berth_id=-1;
        for(int i=0;i<berth_num;i++){
            if(berth_priority[i]==0||maps[i][x][y]==0)
                continue;
            if(maps[i][x][y]<mi){
                select_berth_id=i;
                mi=maps[i][x][y];
            }  
        }
        return select_berth_id;
}
//查找最近且优先级最高港口
int findBerth(int rid){
    int x=robot[rid].x;
    int y=robot[rid].y;
    int ma=-1;
    int mi=0x7fffffff;
    int priority=-1;
    int append1=change_limit;
    int append2=-change_limit;
    int select_berth_id=-1;
    vector<int> robots_num(berth_num,0);
    int tmp_val;
    for(int i=0;i<robot_num;i++)
    {   
        // if(robot[i].goods)
            robots_num[robot[i].berth_id]++;
    }
            
    robots_num[robot[rid].berth_id]--;
    for(int i=0;i<berth_num;i++){
        if(berth_priority[i]==0||priority>berth_priority[i]||maps[i][x][y]==0)
            continue;
        if(robot[rid].berth_id==i){
            append1=change_limit-robots_num[i]*punish_robot;
            append2=-change_limit+robots_num[i]*punish_robot;
        }
        else{
            append1=0-robots_num[i]*punish_robot;
            append2=0+robots_num[i]*punish_robot;
        }
        if(priority<berth_priority[i]){
            select_berth_id=i;
            priority=berth_priority[i];
            switch (berth_priority[i])
            {
                case 1:
                    mi=maps[i][x][y];    // 无欲无求了，去最近的就行
                    break;
                case 2:
                    ma=berth[i].loading_speed; //谁装得快去哪
                break;
                case 3:
                    //ma=goods_nums[i]+berth_sum[i]; //未来发展潜力大的优先
                   // ma=goods_nums[i]+berth_sum[i]+append1;
                    ma=goods_nums[i]+append1;
                break;
                case 4:
                   // mi=maps[i][x][y]; //快点填满吧
                    ma=goods_nums[i]+append1;
                    //ma=goods_nums[i]+berth_sum[i]+append1;
                break;
                
                default:
                    break;
            }
            
        }
        else{
            switch (berth_priority[i])
            {
                    case 1:
                        if(maps[i][x][y]>=mi)
                            break;
                        select_berth_id=i;
                        mi=maps[i][x][y];    // 无欲无求了，去最近的就行
                        break;
                    case 2:
                        if(berth[i].loading_speed<=ma)
                        if(goods_nums[i]+berth_sum[i]+append1<=ma)
                            break;
                        select_berth_id=i;
                        ma=berth[i].loading_speed; //谁装得快去哪
                        
                    break;
                    case 3:
                        //if(goods_nums[i]+berth_sum[i]<=ma)
                        //if(goods_nums[i]+berth_sum[i]+append1<=ma)
                        if(goods_nums[i]+append1<=ma)
                            break;
                        select_berth_id=i;
                        //ma=goods_nums[i]+berth_sum[i]+append1;
                        ma=goods_nums[i]+append1;
                        //ma=goods_nums[i]+berth_sum[i]; //未来发展潜力大的优先
                    break;
                    case 4:
                        // if(maps[i][x][y]>=mi)
                        if(goods_nums[i]+append1<=ma)
                        //if(goods_nums[i]+berth_sum[i]+append1<=ma)
                            break;
                        select_berth_id=i;
                    // mi=maps[i][x][y]; //快点填满吧
                        ma=goods_nums[i]+append1;
                    //ma=goods_nums[i]+berth_sum[i]+append1;
                    break;
                    
                    default:
                        break;
            }
        }
        
    }
    
    return select_berth_id;
}

void tryfindBerth(int rid){
    int priority=berth_priority[robot[rid].berth_id];
    int x=robot[rid].x;
    int y=robot[rid].y;
    if(priority==4){
        int mi=0x7fffffff;
        int selected_berth_id=robot[rid].berth_id;
        for(int i=0;i<berth_num;i++){
            if(berth_priority[i]==priority&&maps[i][x][y]!=0){
                if(maps[i][x][y]<mi){
                    mi=maps[i][x][y];
                    selected_berth_id=i;
                }
            }
        }
        if(selected_berth_id!=robot[rid].berth_id)
        {
            robot[rid].berth_id=selected_berth_id;
            robot[rid].status=10+selected_berth_id;
        }
    }
}

int countTime(int bid,int nums){
    int shortest_step=0;
    int k=nums;
    int init=-1;
    for(auto &it :virtual_heads[bid]){
        if(it.second->status==0)
        {
            shortest_step+=it.first;
            k--;
            if(init==-1){
                init=it.first;
            }
            if(k==0)
                break;
            
        }
    }
    shortest_step+=init*k;//如果数量不足，补充虚拟节点数量
    return shortest_step;
}
//查找其他港口来调度
int findOtherBerth(int bid,int need_goods=0,int pre_steps=0){
    int mi=0x7fffffff;
    int seleted_berth_id=-1;
    int tmp;
    for(int i=0;i<berth_num;i++){
        if(berth_priority[i]==0)
            continue;
        tmp=max(500,countTime(i,need_goods-berth_sum[i]));
        if(tmp==500){
            tmp+=(need_goods+berth[i].loading_speed-1)/berth[i].loading_speed;
        }
        if(berth_status[i]==-1&&(15000-berth[i].transport_time-tmp)>=frame_time){
            
            if(tmp+berth[i].transport_time<mi){
                mi=tmp+berth[i].transport_time;
                seleted_berth_id=i;
            }
        }
    }
    //&&(berth_sum[seleted_berth_id]+goods_nums[seleted_berth_id])>=need_goods
    // if(pre_steps-mi>1000000);
    //     return seleted_berth_id;
    if(pre_steps-mi>save_steps)
       return seleted_berth_id; 
    return -1;
   
}
//返转方向
int reverseDir(int direction){
        switch (direction)
        {
        case 0:
           return 1;
        case 1:
            return 0;
        case 2:
            return 3;
        case 3:
            return 2;
        default:
            return -1;
        }
        return -1;
    }

//随机走一格
vector<int> randomGo(int x,int y){
    vector<int> okdir;
    if(((ch[x+1][y]=='.')||(ch[x+1][y]=='B'))&&gds[x+1][y]<'a')
        okdir.push_back(3);
    if(((ch[x-1][y]=='.')||(ch[x-1][y]=='B'))&&gds[x-1][y]<'a')
        okdir.push_back(2);
    if(((ch[x][y+1]=='.')||(ch[x][y+1]=='B'))&&gds[x][y+1]<'a')
        okdir.push_back(0);
    if(((ch[x][y-1]=='.')||(ch[x][y-1]=='B'))&&gds[x][y-1]<'a')
        okdir.push_back(1);
    return okdir;
}
//空地探测
vector<int> searchPath(int x,int y){
    vector<int> okdir;
    if(((ch[x+1][y]=='.')||(ch[x+1][y]=='B')))
        okdir.push_back(3);
    if(((ch[x-1][y]=='.')||(ch[x-1][y]=='B')))
        okdir.push_back(2);
    if(((ch[x][y+1]=='.')||(ch[x][y+1]=='B')))
        okdir.push_back(0);
    if(((ch[x][y-1]=='.')||(ch[x][y-1]=='B')))
        okdir.push_back(1);
    if(okdir.empty())
        okdir.push_back(-1);
    return okdir;
}

//机器人走到港口的时候执行
void put_goods(int rid){
    stringstream sg;
    sg<<"pull "<<rid<<endl;
    robot_action.push_back(sg.str());
    berth_sum[robot[rid].berth_id]++;
    store.deleteGoods(robot[rid].gr);
    robot[rid].init_goods(store.findGoods(rid));
}
//机器人走到货物上方的时候执行
void pick_goods(int rid){
    stringstream sg;
    sg<<"get "<<rid<<endl;
    robot_action.push_back(sg.str());
    quick_map[robot[rid].gr->x*500+robot[rid].gr->y]=nullptr;
    robot[rid].berth_id=findBerth(rid); //最优查找巷口
    goods_nums[robot[rid].gr->berth_id]--;
    robot[rid].gr->status=2;    
}

void run(int rid,int tar=-1){
        if(robot_status[rid]||robot[rid].berth_id==-1)
            return;
        
        robot_status[rid]=1;
        int x=robot[rid].x;
        int y=robot[rid].y;
        vector<int> idir;
        if(tar<0){
            idir=robot[rid].preRun();  //过滤物理隔绝和历史方向
        }else if(tar>=0)   
        {
            idir=searchPath(x,y);
        }
        if(idir[0]==-1){
            robot[rid].dump++;
            if(robot[rid].dump>=pong_check){
                if(robot[rid].goods){
                    robot[rid].berth_id=findBerthShort(robot[rid].x,robot[rid].y);
                }else{
                    robot[rid].init_goods(store.findGoods(rid));
                }
            }
            return;
        }

        vector<int>okdir;
        for(int i=0;i<idir.size();i++){                
            switch (idir[i])
            {
                case 2:
                    if(gds[x-1][y]<'a')
                        okdir.push_back(2);
                    break;
                case 3:
                    if(gds[x+1][y]<'a')
                        okdir.push_back(3);
                    break;
                case 1:
                    if(gds[x][y-1]<'a')
                        okdir.push_back(1);
                    break;
                case 0:
                    if(gds[x][y+1]<'a')
                        okdir.push_back(0);
                    break;
                default:
                    break;
            }
        }
     
        if(okdir.empty()){//发生了碰撞 撞到了人 
            int rrid;
            //尝试让对方先走，再走自己的 
            int tag=1;
            for(int i=0;i<idir.size()&&tag;i++){
                switch (idir[i])
                {
                    case 2:
                        if(gds[x-1][y]>='a'){
                            rrid=gds[x-1][y]-'a';     
                            if(robot_status[rrid]==0)
                            {
                                if(robot[rrid].goods&&robot[rid].goods)
                                    robot[rrid].berth_id=robot[rid].berth_id;                                
                                robot[rrid].last_direction=2;
                                if(robot[rrid].restore_tag>=2&&robot[rrid].restore_path.top()==reverseDir(2))
                                    run(rrid,2);
                                else
                                    run(rrid,-2);
                                if(gds[x-1][y]<'a')
                                    tag=0,okdir.push_back(2);
                            }
                        }
                        break;
                    case 3:
                        if(gds[x+1][y]>='a'){
                            rrid=gds[x+1][y]-'a';
                            if(robot_status[rrid]==0)
                            {
                                if(robot[rrid].goods&&robot[rid].goods)
                                    robot[rrid].berth_id=robot[rid].berth_id; 
                                robot[rrid].last_direction=3;
                                if(robot[rrid].restore_tag>=2&&robot[rrid].restore_path.top()==reverseDir(3))
                                    run(rrid,3);
                                else
                                    run(rrid,-2);
                                if(gds[x+1][y]<'a')
                                    tag=0,okdir.push_back(3);
                            }
                        }
                        break;
                    case 1:
                        if(gds[x][y-1]>='a'){
                            rrid=gds[x][y-1]-'a'; 
                            if(robot_status[rrid]==0)
                            {
                                if(robot[rrid].goods&&robot[rid].goods)
                                    robot[rrid].berth_id=robot[rid].berth_id;
                                robot[rrid].last_direction=1;
                                if(robot[rrid].restore_tag>=2&&robot[rrid].restore_path.top()==reverseDir(1))
                                    run(rrid,1);
                                else
                                    run(rrid,-2);
                                if(gds[x][y-1]<'a')
                                    tag=0,okdir.push_back(1);
                            }
                        }
                        break;
                    case 0:
                        if(gds[x][y+1]>='a'){
                            rrid=gds[x][y+1]-'a'; 
                            if(robot_status[rrid]==0)
                            {
                                if(robot[rrid].goods&&robot[rid].goods)
                                    robot[rrid].berth_id=robot[rid].berth_id; 
                                robot[rrid].last_direction=0;
                                if(robot[rrid].restore_tag>=2&&robot[rrid].restore_path.top()==reverseDir(0))
                                    run(rrid,0);
                                else
                                    run(rrid,-2);
                                if(gds[x][y+1]<'a')
                                    tag=0,okdir.push_back(0);
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            
            
            //实在不行就特殊处理
            if(okdir.empty()){
                //有货机器人的碰撞，改变港口然后等待就好了 
                if(robot[rid].goods){
                    if(robot[rid].dump==3)
                    {
                        okdir=randomGo(robot[rid].x,robot[rid].y);
                    }
                }
                //考虑空机器人
                else {
                    
                    if(robot[rid].gr!=nullptr&&robot[rid].restore_tag==0)
                    {
                        int pre_dir=robot[rid].path[robot[rid].pointer];
                        //两个按图索骥空机器人的碰撞
                        if((robot[rrid].goods==0&&robot[rrid].gr!=nullptr&&robot[rrid].restore_tag==0&&robot[rrid].path[robot[rrid].pointer]==reverseDir(pre_dir)))
                        {
                            
                            robot[rid].last_direction=reverseDir(pre_dir);
                            robot[rrid].last_direction=pre_dir;

                            Goods *t1=robot[rid].gr;
                            robot[rid].gr=robot[rrid].gr;
                            robot[rrid].gr=t1;
                            
                            vector<int> t2=robot[rid].path;
                            robot[rid].path=robot[rrid].path;
                            robot[rrid].path=t2;

                            int t3=robot[rid].pointer;
                            robot[rid].pointer=robot[rrid].pointer+1;
                            robot[rrid].pointer=t3+1;


                            t3=robot[rid].berth_id;
                            robot[rid].berth_id=robot[rrid].berth_id;
                            robot[rrid].berth_id=t3;
                            
                            pre_dir=robot[rid].path[robot[rid].pointer];
                            switch (pre_dir)
                            {
                                case 2:
                                    if(gds[x-1][y]<'a')
                                        okdir.push_back(2);
                                    break;
                                case 3:
                                    if(gds[x+1][y]<'a')
                                        okdir.push_back(3);
                                    break;
                                case 1:
                                    if(gds[x][y-1]<'a')
                                        okdir.push_back(1);
                                    break;
                                case 0:
                                    if(gds[x][y+1]<'a')
                                        okdir.push_back(0);
                                    break;
                                default:
                                    break;
                            }

                        }
                        
                    }
                  
                    if(tar==-2&&okdir.empty()){
                        if(robot[rid].restore_tag==0)
                            robot[rid].restore_tag=1;
                        okdir=randomGo(robot[rid].x,robot[rid].y);
                    }
                }
                
                
            }
            
            if(okdir.empty()){
                robot[rid].dump++;
                if(robot[rid].restore_tag==1)
                    robot[rid].restore_tag=0;
                if(robot[rid].dump>=pong_check){
                    if(robot[rid].goods){
                        robot[rid].berth_id=findBerthShort(robot[rid].x,robot[rid].y);
                    }else{
                        robot[rid].init_goods(store.findGoods(rid));
                    }
                }
                return;
            }
                     
        }
        robot[rid].dump=0;
        int go=okdir[rand()%okdir.size()];
        char tmp;
        stringstream ss;
        //曾今走过的路，都要一一偿还 tothink
        if(robot[rid].goods==0){
            if(robot[rid].restore_tag==1)
            {
                robot[rid].restore_tag=2;
            }
            if(robot[rid].restore_tag==2||tar!=-1){
                robot[rid].restore_tag=2;
                robot[rid].restore_path.push(reverseDir(go));
            }    
            else if(robot[rid].restore_tag==3){
                robot[rid].restore_path.pop();   
            }
        }
     

        switch (go)
        {
            case 2:
                ss<<"move "<<rid<<" "<<2<<endl;
                robot_action.push_back(ss.str());
                robot[rid].x--;
                robot[rid].last_direction=2;
                if(robot[rid].goods==0&&robot[rid].restore_tag==0&&robot[rid].gr!=nullptr){
                    // //违背祖宗的决定发生后，需要恢复到原来的状态,当前操作是回退操作
                    // if(robot[rid].pointer>0&&robot[rid].path[robot[rid].pointer-1]==reverseDir(2))
                    // {   
                    //     robot[rid].pointer--;     
                    //     if(robot[rid].pointer>0)
                    //         robot[rid].last_direction=robot[rid].path[robot[rid].pointer-1];
                    //     else
                    //         robot[rid].last_direction=-1;
                    // }else{
                        robot[rid].pointer++;
                    //}
                    if(robot[rid].gr->x==x-1&&robot[rid].gr->y==y)
                        pick_goods(rid);
                }
                gds[x-1][y]=gds[x][y];
                gds[x][y]=0;
                if(ch[x-1][y]=='B'&&robot[rid].goods&&berth[robot[rid].berth_id].check(x-1,y)){
                    put_goods(rid);
                }
                break;
            case 3:
                ss<<"move "<<rid<<" "<<3<<endl;
                robot_action.push_back(ss.str());
                robot[rid].x++;
                robot[rid].last_direction=3;
                if(robot[rid].goods==0&&robot[rid].restore_tag==0&&robot[rid].gr!=nullptr){
                    //违背祖宗的决定发生后，需要恢复到原来的状态,当前操作是回退操作
                    // if(robot[rid].pointer>0&&robot[rid].path[robot[rid].pointer-1]==reverseDir(3))
                    // {     
                    //     robot[rid].pointer--;  
                    //     if(robot[rid].pointer>0)
                    //         robot[rid].last_direction=robot[rid].path[robot[rid].pointer-1];
                    //     else
                    //         robot[rid].last_direction=-1;
                    // }else{
                        robot[rid].pointer++;
                    //}
                    if(robot[rid].gr!=nullptr&&robot[rid].gr->x==x+1&&robot[rid].gr->y==y)
                        pick_goods(rid);
                }
                gds[x+1][y]=gds[x][y];
                gds[x][y]=0;
                if(ch[x+1][y]=='B'&&robot[rid].goods&&berth[robot[rid].berth_id].check(x+1,y)){
                    put_goods(rid);
                }
                break;
            case 1:
                ss<<"move "<<rid<<" "<<1<<endl;
                robot_action.push_back(ss.str());
                robot[rid].y--;
                robot[rid].last_direction=1;
                if(robot[rid].goods==0&&robot[rid].restore_tag==0&&robot[rid].gr!=nullptr){
                    //违背祖宗的决定发生后，需要恢复到原来的状态,当前操作是回退操作
                    // if(robot[rid].pointer>0&&robot[rid].path[robot[rid].pointer-1]==reverseDir(1))
                    // {     
                    //     robot[rid].pointer--;   
                    //     if(robot[rid].pointer>0)
                    //         robot[rid].last_direction=robot[rid].path[robot[rid].pointer-1];
                    //     else
                    //         robot[rid].last_direction=-1;
                    // }else{
                        robot[rid].pointer++;
                    //}
                    if(robot[rid].gr!=nullptr&&robot[rid].gr->x==x&&robot[rid].gr->y==y-1)
                        pick_goods(rid);
                }
                gds[x][y-1]=gds[x][y];
                gds[x][y]=0;  
                if(ch[x][y-1]=='B'&&robot[rid].goods&&berth[robot[rid].berth_id].check(x,y-1)){
                    put_goods(rid);
                }
                break;
            case 0:
                ss<<"move "<<rid<<" "<<0<<endl;
                robot_action.push_back(ss.str());
                robot[rid].y++;
                robot[rid].last_direction=0;
                if(robot[rid].goods==0&&robot[rid].restore_tag==0&&robot[rid].gr!=nullptr){
                    //违背祖宗的决定发生后，需要恢复到原来的状态,当前操作是回退操作
                    // if(robot[rid].pointer>0&&robot[rid].path[robot[rid].pointer-1]==reverseDir(0))
                    // {     
                    //     robot[rid].pointer--;   
                    //     if(robot[rid].pointer>0)
                    //         robot[rid].last_direction=robot[rid].path[robot[rid].pointer-1];
                    //     else
                    //         robot[rid].last_direction=-1;
                    // }else{
                        robot[rid].pointer++;
                    //}
                    if(robot[rid].gr!=nullptr&&robot[rid].gr->x==x&&robot[rid].gr->y==y+1)
                        pick_goods(rid);
                }  
                gds[x][y+1]=gds[x][y];
                gds[x][y]=0;
                if(ch[x][y+1]=='B'&&robot[rid].goods&&berth[robot[rid].berth_id].check(x,y+1)){
                    put_goods(rid);
                }
                break;
            default:
                break;
        }
        
        if(robot[rid].restore_tag==3&&robot[rid].restore_path.empty()){
            robot[rid].restore_tag=0;
            if(robot[rid].pointer==0)
                robot[rid].last_direction=-1; 
            else    
                robot[rid].last_direction=robot[rid].path[robot[rid].pointer-1];
        }

     

    }
void undate_robot(){
    //Goods* p=store.head->next;
    int tmp,tmp1;
    for(int i=0;i<robot_num;i++){
        if(robot[i].goods)
        {
            //robot[i].berth_id=findBerth(i);
            tryfindBerth(i);
            continue;
        }
        else if(robot[i].gr==nullptr&&robot[i].berth_id!=-1){
            robot[i].init_goods(store.findGoods(i));
            //robot[i].try_switch_goods(0);
        }
        
    }
}

void undate_goods(){
    Goods *p=store.head->next;
    virtual_heads= vector<set<pair<int,Goods*> > > (berth_num,set<pair<int,Goods*> >());


    set<Goods*> s;
    while(p!=nullptr){
        if(p->status<2&&gds[p->x][p->y]==0){
            gds[p->x][p->y]='G';
        }
        if(frame_time>=p->frame_id+1000&&p->status<2)
            s.insert(p);
        else if(p->status==0){
            for(int i=0;i<berth_num;i++){
                if(berth_priority[i]==0||maps[i][p->x][p->y]==0)
                    continue;
                virtual_heads[i].insert(make_pair(maps[i][p->x][p->y],p));


            }
        }
        p=p->next;
    }
    for(auto it:s){
        if(gds[it->x][it->y]=='G')
            gds[it->x][it->y]=0;
        goods_nums[it->berth_id]--;
        store.deleteGoods(it);
    }
    for(int i=0;i<robot_num;i++){
        if(robot[i].goods==0&&robot[i].berth_id!=-1&& s.count(robot[i].gr)){
            //robot[i].init_goods(store.findGoods(i));
            robot[i].try_switch_goods(1);
        }
    }

}
void undate_boat(){
    for(int i=0;i<5;i++){
        if(boat[i].pos==-1&&boat[i].status==1){
            int ma=0;
            int bid=-1;
            for(int j=0;j<berth_num;j++)
            {
                if(berth_priority[j]==0&&(15000-berth[j].transport_time*2)<=frame_time)
                    continue;
                int nums=berth_sum[j]+goods_nums[j];
                if(berth_status[j]>=0){
                    for(int k=0;k<5;k++){
                        if(boat[k].pos==j)
                            nums-=(boat_capacity-boat[k].num);
                    }
                }
                if(nums>ma)
                {
                    ma=nums;
                    bid=j;
                }
            }
            
            if(ma>start_ship)//todo
            {
                stringstream ss;
                if(berth[i].transport_time>berth[fast_berth_id].transport_time+500){
                    ss<<"ship "<<i<<" "<<fast_berth_id<<endl;
                    // file<<"change start "<<ss.str()<<endl;
                    if(berth_status[fast_berth_id]==-1)
                        berth_status[fast_berth_id]=i;
                    boat[i].pos=fast_berth_id;
                    boat[i].status=0;
                }else{
                    ss<<"ship "<<i<<" "<<bid<<endl;
                    if(berth_status[bid]==-1)
                        berth_status[bid]=i;
                    boat[i].pos=bid;
                    boat[i].status=0;
                }
               
               
                boat_action.push_back(ss.str());
                undate_berth_priority();

            }
        }
        else if(boat[i].pos!=-1){
            switch (boat[i].status)
            {
                case 0:
                    if(berth_status[boat[i].pos]==-1)
                        berth_status[boat[i].pos]=i;
                    break;
                case 1:
                    if(boat[i].num==boat_capacity||(15000-berth[boat[i].pos].transport_time)==frame_time){
                        stringstream ss;
                        if(berth[i].transport_time>berth[fast_berth_id].transport_time+500){
                            ss<<"ship "<<i<<" "<<fast_berth_id<<endl;
                            // file <<" change back "<<ss.str()<<endl;
                            if(berth_status[fast_berth_id]==-1)
                                berth_status[fast_berth_id]=i;
                            berth_status[boat[i].pos]=-1;
                            boat[i].pos=fast_berth_id;
                            boat[i].status=0;
                        }else{
                            ss<<"go "<<i<<endl;
                            berth_status[boat[i].pos]=-1;
                            boat[i].pos=-1;
                            boat[i].num=0;
                            boat[i].status=0;
                        }
                        boat_action.push_back(ss.str());
                        undate_berth_priority();
                    }else{
                        //berth_status[boat[i].pos]=10+i;
                        int bid=boat[i].pos;
                        int tag=0;
                        int k=(boat_capacity-berth_sum[bid]);
                        if(berth_sum[bid]==0){
                            int shortest_step=countTime(bid,k);
                            if(shortest_step>500){
                                int selected_berth_id=findOtherBerth(i,k,shortest_step+berth[bid].transport_time);
                                if(selected_berth_id!=-1){
                                    tag=1;
                                    stringstream ss;
                                    ss<<"ship "<<i<<" "<<selected_berth_id<<endl;
                                    berth_status[selected_berth_id]=i;
                                    berth_status[bid]=-1;
                                    boat[i].pos=selected_berth_id;
                                    boat[i].status=0;
                                    boat_action.push_back(ss.str());
                                    undate_berth_priority();
                                }
                            }
                        }
                        if(tag==0){
                            berth_status[boat[i].pos]=10+i;
                        }
                    }
                    break;
                case 2:
                    if(boat[i].num==boat_capacity||(15000-berth[boat[i].pos].transport_time)==frame_time){
                        stringstream ss;
                        // file<<"well well well"<<endl;
                        if(berth[i].transport_time>berth[fast_berth_id].transport_time+500){
                            ss<<"ship "<<i<<" "<<fast_berth_id<<endl;
                            if(berth_status[fast_berth_id]==-1)
                                berth_status[fast_berth_id]=i;
                            berth_status[boat[i].pos]=-1;
                            boat[i].pos=fast_berth_id;
                            boat[i].status=0;
                        }else{
                            ss<<"go "<<i<<endl;
                            berth_status[boat[i].pos]=-1;
                            boat[i].pos=-1;
                            boat[i].num=0;
                            boat[i].status=0;
                        }
                        boat_action.push_back(ss.str());
                        undate_berth_priority();
                    }else{
                        //berth_status[boat[i].pos]=10+i;
                        // file<<"good  come from start"<<endl;
                        int bid=boat[i].pos;
                        int tag=0;
                        int k=(boat_capacity-berth_sum[bid]);
                        if(berth_sum[bid]==0){
                            int shortest_step=countTime(bid,k);
                            if(shortest_step>500){
                                int selected_berth_id=findOtherBerth(i,k,shortest_step+berth[bid].transport_time);
                                if(selected_berth_id!=-1){
                                    tag=1;
                                    stringstream ss;
                                    ss<<"ship "<<i<<" "<<selected_berth_id<<endl;
                                    berth_status[selected_berth_id]=i;
                                    berth_status[bid]=-1;
                                    boat[i].pos=selected_berth_id;
                                    boat[i].status=0;
                                    boat_action.push_back(ss.str());
                                    undate_berth_priority();
                                }
                            }
                        }
                    }
                    if(berth_status[boat[i].pos]==-1)
                        berth_status[boat[i].pos]=i;
                default:
                    break;
            }
            
        }
            

    }
}
void undate_berth_priority(){
      for(int i=0;i<berth_num;i++){
        if(berth_status[i]>=10&&(15000-berth[i].transport_time)==frame_time)
            berth_priority[i]=0;
        if(berth_status[i]==-1&&(15000-berth[i].transport_time*2)<=frame_time)
            berth_priority[i]=0;
        if(berth_priority[i]==0)
            continue;
        int boat_have=0;
        int capacity_full=0;
        if(berth_status[i]>=0)
            boat_have=1;
        if(boat_have){
            int nums=0;
            for(int j=0;j<5;j++)
                if(boat[j].pos==i)
                    nums+=(boat_capacity-boat[j].num);
            if(nums<=berth_sum[i])
                capacity_full=2;
        
            // int boat_id;
            // if(berth_status[i]>=10)
            //     boat_id=berth_status[i]-10;
            // else 
            //     boat_id=berth_status[i];
            // if(boat[boat_id].num+berth_sum[i]>=boat_capacity)
            //     capacity_full=2;
        }else{
            if(berth_sum[i]>=boat_capacity)
                capacity_full=2;
        }
    
        switch (boat_have+capacity_full)
        {
            case 0:
                berth_priority[i]=3;
                break;
            case 1:
                berth_priority[i]=4;
                break;
            case 2:
                berth_priority[i]=1;
                break;
            case 3:
                berth_priority[i]=2;
                break;
            default:
                break;
        }
   }
}
void undate_berth(){
    for(int bid=0;bid<berth_num;bid++){
        if(berth_status[bid]<10)
            continue;
        int tmp;
        if(berth_sum[bid]<=berth[bid].loading_speed)
            tmp=berth_sum[bid];
        else
            tmp=berth[bid].loading_speed;
        tmp=min(tmp,boat_capacity-boat[berth_status[bid]-10].num);
        boat[berth_status[bid]-10].num+=tmp;
        berth_sum[bid]-=tmp;
    }
    undate_berth_priority();
}



int main()
{

    Init();

    for(int zhen = 1; zhen <= 15000; zhen ++)
    {

        Input();

        undate_goods();
        
        undate_robot();

        undate_boat();

        for(int i=0;i<robot_num;i++){
            if(robot[i].goods)
                run(i);
        }

        for(int i=0;i<robot_num;i++){
            if(robot[i].goods==0)
                run(i);
        }
       
        undate_berth();

        for(int i=0;i<robot_action.size();i++){
            printf("%s",robot_action[i].c_str());
        }
        for(int i=0;i<boat_action.size();i++){
            printf("%s",boat_action[i].c_str());
        }
        // if(zhen%100==0){
        //     for(int i=0;i<10;i++){
        //        file<<"berth  id "<<i<<" status "<<berth_status[i]<<" priority "<< berth_priority[i]<<"  nums "<<berth_sum[i]<<endl;
        //     }
        //     for(int i=0;i<5;i++){
        //         file<<"boat id="<<i<<"  status "<<boat[i].status<<"  pos "<<boat[i].pos<<"   nums "<<boat[i].num<<endl;
        //     }
        //      for(int i=0;i<10;i++){
        //        file<<"robot  id "<<i<<" goods "<<robot[i].goods<<" gr "<< robot[i].gr<<"  berth id "<<robot[i].berth_id<<endl;
        //     }
        //     file<<"---------------------------------------"<<endl;
        // }
        puts("OK");
        fflush(stdout);
    }
    return 0;
}
