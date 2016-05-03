/***************************************************************************
                    Copyright @ sjtu
                    time: 2016.3.15-31-9
****************************************************************************/

#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <queue>
#include <vector>
#include <fstream>
#include <sstream>
#include <set>
#include <thread>
#include <atomic>




using namespace std;
const int NumOfNodes = 68;
const int INF_MAX = 2147483647;

// normal use KB
const long long CapOfLink = 500 * 1024;  //500Mb/s
const long long Size_object = 5 * 1024; // 5MB
const long long Size_Bs = 2 * 1024 * 1024; //2GB
const int scale = 1;

const int NumofObj = 3000;  // the number of files
const int Total_Time = 15250; //100 sec, 80ms in virtual
const int back_upp = (CapOfLink / 8)/ Size_object;
const int File_Max = 2500000;
const int Min_Time = 10000;
const double alpha_n_k_max = 30;
double W = 10000;
const double Q_n_k_max = 200;
const double r_n_k = CapOfLink;



const double SHRINK_RATIO = 0.5;



// MULTITHREAD
#define MULTITHREAD

#define THREAD_NUM 4
atomic_int TASK_ID;
thread thd[THREAD_NUM];
atomic_int cnt1;
atomic_int cnt2;


#define READ_JUMP_NUM 0







double sum_a_k[(NumOfNodes + 1)*(NumofObj + 1)] = {0};




int linked[NumOfNodes + 1][NumOfNodes + 1] = {0};
int A_n_k[NumOfNodes + 1][NumofObj + 1] = {0}; //A_n^k[n][k]
float v_a_n_k[NumOfNodes + 1][NumOfNodes + 1][NumofObj + 1] = {0};  //v_a_n_k[a][n][k]
float sum_v_a_n_k[NumOfNodes + 1][NumOfNodes + 1][NumofObj + 1] = {0}; // average ..
int Total_Request[NumOfNodes + 1][2000] = {0};

double ObjProb[NumofObj + 1] = {0};
float mu_a_b_k[NumOfNodes + 1][NumOfNodes + 1][NumofObj + 1] = {0}; //[a][b][k]

atomic<unsigned long long> delay;
atomic_int hit;
atomic_int has_finished;
atomic_int Files;
int Cur_Time = 0;
int Src[NumofObj + 1] = {0};
int neigh[NumOfNodes + 1][NumOfNodes + 1] = {0};
int dis_src[NumOfNodes + 1][NumOfNodes + 1] = {0}; //record of the node to destination(k)

int Cur_Turn = 0;
int tempmaps[NumOfNodes + 1][NumOfNodes + 1] = {0};
int record[NumOfNodes * 5] = {0};
int Min_Len = INF_MAX;
int path[NumOfNodes + 1][NumOfNodes + 1][NumOfNodes + 1] = {0};
double ratio_z = 1;
double delta = 0;
double Total_Utility = 0;
double sum_a_n_k[NumOfNodes + 1][NumofObj + 1] = {0};
int clients[Total_Time * NumOfNodes + 10] = {0};

atomic_int signal_p;
atomic_int Trans_Remain;
FILE* read_file;

struct data_obj
{
    double acc;
    double acc_buffer;
    int s;

    data_obj():acc(0),s(0),acc_buffer(0)   {}
};

struct Interest_packet
{
    int content;
    int signature;
    int directed;

    Interest_packet(int c = -1,int s = -1):content(c),signature(s),directed(0) {}
};


Interest_packet BUF_pit_buffer[NumOfNodes+1][100*(NumofObj+1)];
atomic_int buf_itr[NumOfNodes+1];

struct Data_packet
{
    int content;

    Data_packet(int c = -1):content(c){}
};

class nodes
{
public:
    int index,used,size,used_buffer;
    int buffer_size;
    atomic<double> CS[NumofObj + 1]; // record of the score of object k
    int In_PIT[NumofObj + 1];
    int sign_identify[File_Max];
    int FIB[NumofObj + 1][NumOfNodes + 1];
    int FIB_buffer[NumofObj + 1][NumOfNodes + 1];

    int alpha[NumofObj + 1];
    double gamma[NumofObj + 1];
    double Y_n_k[NumofObj + 1];
    double Q_n_k[NumofObj + 1];

    data_obj data_que[NumofObj + 1];  // record of data cached in the node
    Data_packet back_que_buffer[File_Max];

    long long time_record[NumofObj + 1][2];


    queue<Interest_packet> PIT;
    queue<Data_packet> back_que;
    queue<Interest_packet> PIT_buffer;

    nodes(int kk = -1,unsigned int ss = -1):index(kk),size(ss),used(0),used_buffer(0){
        buffer_size = 0;
        memset(CS,0,sizeof(CS));
        memset(In_PIT,0,sizeof(In_PIT));
        memset(sign_identify,0,sizeof(sign_identify));
        memset(FIB,0,sizeof(FIB));
        memset(FIB_buffer,0,sizeof(FIB_buffer));
        memset(time_record,0,sizeof(time_record));
        memset(Y_n_k,0,sizeof(Y_n_k));
        memset(alpha,0,sizeof(alpha));
        memset(gamma,0,sizeof(gamma));
        memset(Q_n_k,0,sizeof(Q_n_k));
    }

    void Clear_FIB_Buffer(){memset(FIB_buffer,0,sizeof(FIB_buffer));}
    void clear_signal() {memset(sign_identify,0,sizeof(sign_identify));}
};

nodes NodeArr[NumOfNodes + 1];

struct transport
{
    queue<int> buffer;
    int in_buffer[NumofObj + 1];

    transport() {memset(in_buffer,0,sizeof(in_buffer));}
};

transport TransBuffer[NumOfNodes + 1];

struct request
{
    int time;
    int content;

    request(int i = 0,int y = 0):time(i),content(y)    {}
};

struct request_list
{
    vector<request> rlist;
};

request_list rmaps[NumOfNodes + 1][NumOfNodes + 1];

bool cmp(data_obj &a,data_obj &b)
{
    return a.acc > b.acc;
}

struct BackQue
{
    queue<Data_packet> back_que;
    int in_list[NumofObj + 1];

    BackQue(){memset(in_list,0,sizeof(in_list));}
};

BackQue queue_list[NumOfNodes + 1][NumOfNodes + 1];

void Reversed_link()
{
    int copymaps[NumOfNodes + 1][NumOfNodes + 1] = {0};

    for(int i = 0; i < NumOfNodes; ++i)
        for(int k = 0; k < NumOfNodes; ++k)
            if(linked[i][k])
                copymaps[i + 1][k + 1] = linked[i][k];

    memset(linked,0,sizeof(linked));

    for(int i = 1; i <= NumOfNodes; ++i)
        for(int k = 1; k <= NumOfNodes; ++k)
            if(copymaps[i][k])
                linked[i][k] = copymaps[i][k];

    for(int i = 1; i <= NumOfNodes; ++i)
        for(int k = 1; k <= NumOfNodes; ++k)
            linked[i][k] |= linked[k][i];

    for(int i = 1; i <= NumOfNodes; ++i)
        for(int k = 1; k <= NumOfNodes; ++k)
            if(linked[i][k]){
                neigh[i][0] += 1;
                neigh[i][neigh[i][0]] = k;
            }
}

void addLink(int a,int b)
{
    linked[a][b] = linked[b][a] = 1;
}

void Initalize_Topology()
{
     //Geant Topology

    FILE *topo = fopen("DTelekom.topo","r");
    int a,b;
    while(~fscanf(topo,"%d %d",&a,&b)){
        addLink(a,b);
    }
    fclose(topo);
    for(int i = 1; i <= NumOfNodes; ++i)
        NodeArr[i].size = Size_Bs;

    // bidirect graph
    Reversed_link();

    for(int i = 1; i <= NumOfNodes; ++i)
        for(int k = 1; k <= NumOfNodes; ++k)
            tempmaps[i][k] = linked[i][k];

    // Abilene Topology
    FILE* fsrc;
    fsrc = fopen("./DTelekom_src.txt","r");
    int index;
    for(int i = 1; i <= NumofObj; ++i){
        fscanf(fsrc,"%d",&index);
        Src[i] = index;
        NodeArr[Src[i]].data_que[i].s = 1;
    }
    fclose(fsrc);
}

/*================== Auxiliary Functions ===========================*/
inline double min_(double a,double b)
{
    return (a-b > 0.01 ? b:a);
}

inline double max_(double a,double b){
    return (a-b>0 ? a:b);
}

inline void Read_File(int cur_node,int num)
{
    int index = 0,temp;

    while(index < num){
        if(~fscanf(read_file,"%d",&temp)){
            if(temp <= NumofObj)
                Total_Request[cur_node][index++] = temp;
        }
        else{
            fseek(read_file,0L,SEEK_SET);
        }
    }
}


// inline double Bias_func(int node,int content)
// {
//     if(ratio_z == 0) return delta * dis_src[node][Src[content]];
//     /*double temp = Dijkstra(node,Src[content],content);
//     if(temp)
//         cout << "# " << temp << endl;
//     return (ratio_z * temp + delta * dis_src[node][Src[content]]);*/
//     double sum = 0,num = 0;
//     double H = INF_MAX;

//     for(int k = 1; k <= neigh[node][0]; ++k){
//         int next = neigh[node][k];
//         H = min_(H,NodeArr[next].data_que[content].acc);
//     }
//     return (H * ratio_z + delta * dis_src[node][Src[content]]);

//     if(ratio_z != 0){
//         for(int k = 1; k <= neigh[node][0]; ++k){
//             int next = neigh[node][k];
//             sum += NodeArr[next].data_que[content].acc;
//         }

//         sum /= neigh[node][0];
//     }

//     return (sum * ratio_z + delta * dis_src[node][Src[content]]);
// }


inline double Bias_func(int node,int content,int excl)
{
    if(ratio_z == 0) return delta * dis_src[node][Src[content]];
    /*double temp = Dijkstra(node,Src[content],content);
    if(temp)
        cout << "# " << temp << endl;
    return (ratio_z * temp + delta * dis_src[node][Src[content]]);*/
    double sum = 0,num = 0;
    double H = INF_MAX;

    for(int k = 1; k <= neigh[node][0]; ++k){
        int next = neigh[node][k];
        if (next == excl) continue;
        // H = min_(H,noshrink_acc[next][content]);
        H = min_(H,(NodeArr[next].data_que[content].acc) );
        // H = min_(H,(NodeArr[next].data_que[content].acc<=1)?NodeArr[next].data_que[content].acc:0.5*(NodeArr[next].data_que[content].acc-1)+1 );
        // H = min_(H,noshrink_acc[next][content]);
    }
    if (H == INF_MAX) H = 0;
    return (H * ratio_z + delta * dis_src[node][Src[content]]);

    if(ratio_z != 0){
        for(int k = 1; k <= neigh[node][0]; ++k){
            int next = neigh[node][k];
            // sum += ((NodeArr[next].data_que[content].acc<=1)?NodeArr[next].data_que[content].acc:0.5*(NodeArr[next].data_que[content].acc-1)+1);
            sum += (NodeArr[next].data_que[content].acc);
        }

        sum /= neigh[node][0];
    }

    return (sum * ratio_z + delta * dis_src[node][Src[content]]);
}

inline double Positive(double b)
{
    return (b > 0 ? b:0);
}

void DFS_Traverse(int start,int end,int src,int length = 0)
{
    int i,j;
    for(i = 1; i <= NumOfNodes; ++i){
        if(tempmaps[start][i])
        {
            if(i == end)/*if the target*/
            {
                if(length < Min_Len){
                    path[src][end][0] = src;
                    for(j = 1; j <= length; ++j)
                        path[src][end][j] = record[j - 1];
                    path[src][end][j] = end;
                    path[src][end][j + 1] = 0;
                    Min_Len = length;
                    dis_src[src][end] = dis_src[end][src] = Min_Len + 1;
                }
                return;
            }
            else{
                record[length] = i;
                ++length;
                if(length > Min_Len || length > NumOfNodes)
                    return;
                tempmaps[start][i] = 0;
                DFS_Traverse(i,end,src,length);
                tempmaps[start][i] = 1;
                --length;
            }
        }
    }
}

void DFS(int start,int end)
{
    if(start == end)
        return;
    Min_Len = INF_MAX;
    DFS_Traverse(start,end,start);
}

void Init_Distance()
{
    for(int i = 1;i <= NumOfNodes; ++i)
        for(int k = 1; k <= NumOfNodes; ++k)
            DFS(i,k);
}


/*================== Algorithm of Congestion Control ===============*/

/*================== Algorithm of Congestion Control ===============*/

void congestion_control(int tid){
    int myID;
    double temp = 0;
    int file,queue_size,num_in;
    while((myID=TASK_ID++)<=NumOfNodes){
        int i,n; i = n = myID;
        for(int k = 0; Total_Request[i][k]; ++k){
            Trans_Remain += 1;
            A_n_k[i][Total_Request[i][k]] += scale;

            file = Total_Request[i][k];
            if(TransBuffer[i].in_buffer[file]){
                TransBuffer[i].in_buffer[file] += 1;
            }
            else{
                TransBuffer[i].buffer.push(file);
                TransBuffer[i].in_buffer[file] = 1;
            }
        }

         for(int k = 1; k <= NumofObj; ++k){
            if(NodeArr[n].Y_n_k[k] > NodeArr[n].data_que[k].acc){
                NodeArr[n].alpha[k] = min_(NodeArr[n].Q_n_k[k],alpha_n_k_max);
            }
            else{
                NodeArr[n].alpha[k] = 0;
            }
            if(NodeArr[n].Y_n_k[k] <= 0)
                NodeArr[n].gamma[k] = alpha_n_k_max;
            else{
                temp = sqrt(W/NodeArr[n].Y_n_k[k]);
                if(temp > alpha_n_k_max){
                    NodeArr[n].gamma[k] = alpha_n_k_max;
                }
                else{
                    NodeArr[n].gamma[k] = temp;
                }
            }

            sum_a_n_k[n][k] += NodeArr[n].alpha[k];

            NodeArr[n].Q_n_k[k] = min_(Positive(NodeArr[n].Q_n_k[k] - NodeArr[n].alpha[k]) + A_n_k[n][k],Q_n_k_max);
            NodeArr[n].Y_n_k[k] = Positive(NodeArr[n].Y_n_k[k] - NodeArr[n].alpha[k]) + NodeArr[n].gamma[k];
        }


    }
}
void Congestion_Control()
{

    TASK_ID = 1;
    // Transport layer
    #ifdef MULTITHREAD
    for (int i=0;i<THREAD_NUM;++i){
        thd[i] = thread(congestion_control,i);
    }
     for (int i=0;i<THREAD_NUM;++i){
        thd[i].join();
    }
    #else
    congestion_control(0);
    #endif
}


/*================== Algorithm in the virtual layer=================*/
//Update the value of V(t+1)

void update_vt(int tid){
    int n;
    int b;
    while((n = TASK_ID++)<=NumOfNodes){
        for(int k = 1; k <= NumofObj; ++k){
            if(Src[k] == n)
                {NodeArr[n].data_que[k].acc_buffer = 0;continue;}

            for(int m = 1; m <= neigh[n][0]; ++m){
                b = neigh[n][m];
                NodeArr[n].data_que[k].acc_buffer -= v_a_n_k[n][b][k];        // n->b
            }
        // if (Cur_Time >100 &&  NodeArr[n].data_que[k].acc_buffer!=0)printf("%lf\n", NodeArr[n].data_que[k].acc_buffer);
            NodeArr[n].data_que[k].acc_buffer = Positive(NodeArr[n].data_que[k].acc_buffer);
           

            NodeArr[n].data_que[k].acc_buffer += A_n_k[n][k];//NodeArr[n].alpha[k];
            NodeArr[n].data_que[k].acc = 0;
            for(int m = 1; m <= neigh[n][0]; ++m){ //a->n
                int a = neigh[n][m];
                NodeArr[n].data_que[k].acc += v_a_n_k[a][n][k];
            }
            // NodeArr[n].data_que[k].acc_buffer += (NodeArr[n].data_que[k].acc<=1?NodeArr[n].data_que[k].acc:0.4*log(NodeArr[n].data_que[k].acc)+1);
            NodeArr[n].data_que[k].acc_buffer += (SHRINK_RATIO*NodeArr[n].data_que[k].acc);
            // NodeArr[n].data_que[k].acc_buffer += (NodeArr[n].data_que[k].acc<=1?NodeArr[n].data_que[k].acc:1);
            // NodeArr[n].data_que[k].acc_buffer += (NodeArr[n].data_que[k].acc<=1?NodeArr[n].data_que[k].acc:0.1*(NodeArr[n].data_que[k].acc-1)+1);
            NodeArr[n].data_que[k].acc = 0;
            NodeArr[n].data_que[k].acc_buffer = Positive(NodeArr[n].data_que[k].acc_buffer - r_n_k * NodeArr[n].data_que[k].s);
            // if (NodeArr[n].data_que[k].acc_buffer>=1) 
            //     NodeArr[n].data_que[k].acc_buffer = 0.8*(NodeArr[n].data_que[k].acc_buffer-1)+1;

        }
    }
}

void Update_Vt()
{
    int b;
    TASK_ID = 1;
    #ifdef MULTITHREAD
    for (int i=0;i<THREAD_NUM;++i){
        thd[i] = thread(update_vt,i);
    }
     for (int i=0;i<THREAD_NUM;++i){
        thd[i].join();
    }
    #else
    update_vt(0); 
    #endif
}




void update_v_a_n_k(int tid){
    int myID;
    int next;
    while((myID=TASK_ID++)<=NumOfNodes){
        int i,a; i = a = myID;
        for(int k = 1; k <= NumofObj; ++k){

            for(int m = 1; m <= neigh[i][0]; ++m){
                next = neigh[i][m];
                sum_a_k[i*(NumofObj + 1)+k] += mu_a_b_k[i][next][k];
            }
            // if (sum_a_k[i*(NumofObj + 1)+k] == 0) printf("dsdsdsdsd\n");
        }
        int n;



        for(int m = 1; m <= neigh[a][0]; ++m){
            n = neigh[a][m];
            for(int k = 1; k <= NumofObj; ++k){
                if(sum_a_k[a*(NumofObj + 1)+k])
                    v_a_n_k[a][n][k] = (mu_a_b_k[a][n][k]/double(sum_a_k[a*(NumofObj + 1)+k])) * (NodeArr[i].data_que[k].acc_buffer);
                    // v_a_n_k[a][n][k] = (mu_a_b_k[a][n][k]/double(sum_a_k[a*(NumofObj + 1)+k])) * (NodeArr[i].data_que[k].acc_buffer==0.0?0.0:max(0.5*(NodeArr[i].data_que[k].acc_buffer-1)+1,0.0));
                sum_v_a_n_k[a][n][k] += v_a_n_k[a][n][k];
                // if (!(NodeArr[n].data_que[k].s && NodeArr[a].data_que[k].s))
                    // NodeArr[n].CS[k] = NodeArr[n].CS[k] + v_a_n_k[a][n][k];
                    NodeArr[n].CS[k] = NodeArr[n].CS[k] + (v_a_n_k[a][n][k])*pow(1.0/k,0.7);   
                    // NodeArr[n].CS[k] = NodeArr[n].CS[k] + (v_a_n_k[a][n][k]);   
            }
        }



    }
}
void Update_v_a_n_k()
{
    memset(sum_a_k,0,sizeof(sum_a_k));
    int next;
    TASK_ID = 1;
    // for (int i=1;i<=NumOfNodes;++i)
    //     for (int k=1; k<=NumofObj;++k)
    // if (NodeArr[i].data_que[k].acc_buffer>=1) 
    //         NodeArr[i].data_que[k].acc_buffer = 0.99*(NodeArr[i].data_que[k].acc_buffer-1)+1;
    // for (int i=1;i<=NumOfNodes;++i)
    //     for (int j=1;j<=NumofObj;++j)
    //             NodeArr[i].CS[j] = NodeArr[i].CS[j]*1.1;
    

    // #ifdef MULTITHREAD
    // for (int i=0;i<THREAD_NUM;++i){
    //     thd[i] = thread(update_v_a_n_k,i);
    // }
    //  for (int i=0;i<THREAD_NUM;++i){
    //     thd[i].join();
    // }
    // #else
    update_v_a_n_k(0);
    // #endif

}


//Forwarding...Consumer 1->5,2->6,3->7,4->8

void virtual_forwarding(int tid){
    int data_index,b;
    double maximum,temp;
    int myID;
    while((myID=TASK_ID++)<=NumOfNodes){
        int a = myID;
        for(int m = 1; m <= neigh[a][0]; ++m){
            b = neigh[a][m];
            data_index = -1, maximum = 0;

            for(int k = 1; k <= NumofObj; ++k){
                temp = (NodeArr[a].data_que[k].acc + Bias_func(a,k,b)) - (NodeArr[b].data_que[k].acc + Bias_func(b,k,a));
                if(temp > maximum){
                    data_index = k; //k*_a_b
                    maximum = temp;
                }
            }

            if(maximum > 0){ // W*_a_b > 0
                mu_a_b_k[a][b][data_index] = back_upp;
            }
        }
    }

}

void Virtual_Forwarding()
{
    TASK_ID = 1;
    // printf("zhiqian\n");

    #ifdef MULTITHREAD
    for (int i=0;i<THREAD_NUM;++i){
        thd[i] = thread(virtual_forwarding,i);
    }
     for (int i=0;i<THREAD_NUM;++i){
        thd[i].join();
    }
    #else
    virtual_forwarding(0);
    #endif
    // printf("zhihou\n");

   Update_v_a_n_k();

    // printf("zaihou\n");
    //cout << "Virtual_Forwarding Ok !" << endl;
}

//Caching... Algorithm 2
void Caching(int k,int node)
{
    // if enough space
    if(NodeArr[node].used + Size_object <= Size_Bs){
        NodeArr[node].data_que[k].s = 1;
        NodeArr[node].used += Size_object;
    }
    else{
        int index_k_old = -1;
        double minimum = INF_MAX;

        // if score higher, then evicted....
        for(int i = 1; i <= NumofObj; ++i){
            if(NodeArr[node].data_que[i].s && minimum >= NodeArr[node].CS[i] && Src[i] != node){
                index_k_old = i;
                minimum = NodeArr[node].CS[i];
            }
        }

        if (index_k_old == -1) return;

        if(NodeArr[node].CS[k] > minimum){
            if (NodeArr[node].CS[k] > minimum) cnt1++;else cnt2++;
            NodeArr[node].data_que[k].s = 1;
            NodeArr[node].data_que[index_k_old].s = 0;
        }
    }
}

int unchange = 0,change = 0;

/*================== Algorithm in the actual layer =================*/
void Actual_Forwarding_packet(int node,Interest_packet packet)
{
    int b_n_k = node;
    double maximum = -1;
    int content = packet.content;
    int nnode = path[node][Src[content]][1];


    for(int m = 1; m <= neigh[node][0]; ++m){
        int k = neigh[node][m];
        if(queue_list[k][node].in_list[content])
            return;
    }

    if(NodeArr[node].data_que[content].s){
        NodeArr[node].back_que.push(Data_packet(content));
        return;
    }
    NodeArr[node].In_PIT[content] = 1;

    for(int m = 1; m <= neigh[node][0]; ++m){   // forwarding algorithm
        int i = neigh[node][m];
        if(sum_v_a_n_k[node][i][content] > maximum){
            b_n_k = i;
            maximum = sum_v_a_n_k[node][i][content];
        }
    }


    if(NodeArr[b_n_k].sign_identify[packet.signature])  // loop
        packet.directed = 1;

    if(packet.directed == 1){               // shortest path to src(k)
        packet.directed = 0;
        if(NodeArr[nnode].data_que[content].s){
            NodeArr[nnode].back_que_buffer[NodeArr[nnode].buffer_size++] = Data_packet(content);
            NodeArr[nnode].FIB_buffer[content][node] = 1;
        }
        else if(NodeArr[nnode].In_PIT[content]){
            NodeArr[nnode].FIB_buffer[content][node] = 1;
            NodeArr[nnode].PIT_buffer.push(packet);
            // BUF_pit_buffer[nnode][buf_itr[nnode]++] = packet;
            NodeArr[nnode].sign_identify[packet.signature] = 1;
        }
        else{
            NodeArr[nnode].PIT_buffer.push(packet);
            // BUF_pit_buffer[nnode][buf_itr[nnode]++] = packet;
            NodeArr[nnode].FIB_buffer[content][node] = 1;
            NodeArr[nnode].sign_identify[packet.signature] = 1;
        }
        return;
    }
    // NodeMtx[nnode-1].unlock();
    // NodeMtx[node-1].lock();

    if(NodeArr[b_n_k].data_que[content].s)   // find the host to send data packet
    {
        NodeArr[b_n_k].back_que_buffer[NodeArr[b_n_k].buffer_size++] = Data_packet(content);
        NodeArr[b_n_k].FIB_buffer[content][node] = 1;
    }
    else{       // no related data here
        if(NodeArr[b_n_k].In_PIT[content])  // then suppress but add an int erface
        {
            NodeArr[b_n_k].sign_identify[packet.signature] = 1;
            NodeArr[b_n_k].PIT_buffer.push(packet);
            // BUF_pit_buffer[b_n_k][buf_itr[b_n_k]++] = packet;
            NodeArr[b_n_k].FIB_buffer[content][node] = 1;
        }
        else{  // if not in PIT, add to the end of
            NodeArr[b_n_k].sign_identify[packet.signature] = 1;
            NodeArr[b_n_k].PIT_buffer.push(packet);
            // BUF_pit_buffer[b_n_k][buf_itr[b_n_k]++] = packet;
            NodeArr[b_n_k].FIB_buffer[content][node] = 1;
        }
    }
}

// deal with the data packet back process

void actual_back(int tid){
    int visited[NumofObj + 1] = {0};
    int myID;
    while((myID=TASK_ID++)<=NumOfNodes){
        int i = myID;
        memset(visited,0,sizeof(visited));
        int temp;
        while(!NodeArr[i].back_que.empty()){
            temp = (NodeArr[i].back_que.front()).content;
            NodeArr[i].back_que.pop();
            if(visited[temp])   continue;
            visited[temp] = 1;

            int cur_data = temp;

            if(NodeArr[i].FIB[cur_data][i]){    // the self-providing doesn't count the bandwidth
                delay += (Cur_Time * NodeArr[i].time_record[cur_data][1] - NodeArr[i].time_record[cur_data][0]);
                has_finished += NodeArr[i].time_record[cur_data][1];
                NodeArr[i].time_record[cur_data][0] = NodeArr[i].time_record[cur_data][1] = 0;
                NodeArr[i].FIB[cur_data][i] = 0;
            }

            for(int m = 1; m <= neigh[i][0]; ++m){  //enough bandwidth
                int b = neigh[i][m];
                if(NodeArr[i].FIB[cur_data][b]){
                    if(queue_list[i][b].in_list[cur_data] == 0){
                        queue_list[i][b].back_que.push(Data_packet(cur_data));
                        queue_list[i][b].in_list[cur_data] = 1;
                    }
                    NodeArr[i].FIB[cur_data][b] = 0;
                }
            }

            if(!NodeArr[i].data_que[cur_data].s && Src[cur_data] != i)
                Caching(cur_data,i);

            NodeArr[i].In_PIT[cur_data] = 0;
        }
    }

}

void Actual_Back()
{
    TASK_ID = 1;
    #ifdef MULTITHREAD
    for (int i=0;i<THREAD_NUM;++i){
        thd[i] = thread(actual_back,i);
    }
     for (int i=0;i<THREAD_NUM;++i){
        thd[i].join();
    }
    #else
    actual_back(0);
    #endif
    //cout << "Actual Back Ok !" << endl;
}


void release_buffer(int tid){
    int myID;
    int b;

    int visited[NumofObj + 1];
    while((myID=TASK_ID++)<=NumOfNodes){
        int i = myID;
        for(int m = 1; m <= neigh[i][0]; ++m){
            b = neigh[i][m];
            for(int k = 1; k <= back_upp && !queue_list[b][i].back_que.empty(); ++k){
                int content = queue_list[b][i].back_que.front().content;
                NodeArr[i].back_que.push(queue_list[b][i].back_que.front());
                queue_list[b][i].in_list[content] = 0;
                queue_list[b][i].back_que.pop();
            }
        }
        for(int k = 0; k < NodeArr[i].buffer_size; ++k)
            NodeArr[i].back_que.push(Data_packet(NodeArr[i].back_que_buffer[k].content));

        NodeArr[i].buffer_size = 0;
        for(int k = 1; k <= NumofObj; ++k){
            for(int m = 1; m <= NumOfNodes; ++m)
                NodeArr[i].FIB[k][m] |= NodeArr[i].FIB_buffer[k][m];
        }


        Interest_packet temp;
        memset(visited,0,sizeof(visited));
        while(!NodeArr[i].PIT_buffer.empty()){
            temp = NodeArr[i].PIT_buffer.front();
            NodeArr[i].PIT_buffer.pop();
            if(!visited[temp.content])
                NodeArr[i].PIT.push(temp);
            visited[temp.content] = 1;
        }
    }
}
void Release_Buffer()
{
    TASK_ID = 1;
    #ifdef MULTITHREAD
    for (int i=0;i<THREAD_NUM;++i){
        thd[i] = thread(release_buffer,i);
    }
     for (int i=0;i<THREAD_NUM;++i){
        thd[i].join();
    }
    #else
    release_buffer(0);
    #endif
}


void actual_request(int tid){
    int file,queue_size,num_in = 1;
    int myID;
    int b;

    int visited[NumofObj + 1];
    while((myID=TASK_ID++)<=NumOfNodes){
        int i = myID;
        //queue_size = TransBuffer[i].buffer.size();

        for(int k = 0;Total_Request[i][k]; ++k){
            file = Total_Request[i][k];//TransBuffer[i].buffer.front();
            /*TransBuffer[i].buffer.pop();

            if(!NodeArr[i].alpha[file]){
                TransBuffer[i].buffer.push(file);
                continue;
            }

            if(TransBuffer[i].in_buffer[file] > NodeArr[i].alpha[file]){ // can't be admitted in all
                TransBuffer[i].buffer.push(file);
                TransBuffer[i].in_buffer[file] -= NodeArr[i].alpha[file];
                num_in = NodeArr[i].alpha[file];
            }
            else{
                num_in = TransBuffer[i].in_buffer[file];
                TransBuffer[i].in_buffer[file] = 0;
            }*/

            Files += num_in;
           // Trans_Remain -= num_in;

            if(NodeArr[i].data_que[file].s){ // hit in the local cache
                if(Src[file] != i)
                    hit += num_in;
                has_finished += num_in;
            }
            else if(NodeArr[i].In_PIT[file]){   // suppress in the local PIT
                NodeArr[i].FIB[file][i] = 1;
                NodeArr[i].time_record[file][0] += (num_in * Cur_Time);
                NodeArr[i].time_record[file][1] += num_in;  // a more request
            }
            else{                               // Forwarding
                NodeArr[i].FIB[file][i] = 1;

                NodeArr[i].sign_identify[signal_p] = 1;
                NodeArr[i].In_PIT[file] = 1;

                NodeArr[i].time_record[file][0] += (num_in * Cur_Time);
                NodeArr[i].time_record[file][1] += num_in;  // a more request
                NodeArr[i].PIT.push(Interest_packet(file,signal_p++));
            }
        }
    }

}
void Actual_Request()
{

    TASK_ID =1;
    // #ifdef MULTITHREAD
    // for (int i=0;i<THREAD_NUM;++i){
    //     thd[i] = thread(actual_request,i);
    // }
    //  for (int i=0;i<THREAD_NUM;++i){
    //     thd[i].join();
    // }
    // #else
    actual_request(0);
    // #endif
    //cout << "Actual Forwarding Ok !" << endl;
}
/*========================== Request Process =======================*/

// void actual_forwarding(int tid){
//     int i,cnt;
//     Interest_packet temp;
//     int visited[NumofObj + 1];
//     // for (int i=1;i<=NumOfNodes;++i) NodeMtx[i] = 0;

//     while((i=TASK_ID++)<=NumOfNodes){
//         memset(visited,0,sizeof(visited));
//         while(!NodeArr[i].PIT.empty()){
//             temp = NodeArr[i].PIT.front();
//             NodeArr[i].PIT.pop();
//             if(visited[temp.content])
//                 continue;
//             visited[temp.content] = 1;
//             // while(NodeMtx[i] || NodeMtx[path[i][Src[temp.content]][1]]);
//             // NodeMtx[i] = NodeMtx[path[i][Src[temp.content]][1]] = true;
//             // if (NodeMtx[i-1].try_lock() && NodeMtx[path[i][Src[temp.content]][1]-1].try_lock()){
//                 Actual_Forwarding_packet(i,temp);
//             // }else{
//                 // goto JUMP:;
//             // }
//             // NodeMtx[i] = NodeMtx[path[i][Src[temp.content]][1]] = false;
//             // NodeMtx[i-1].unlock();
//             // NodeMtx[path[i][Src[temp.content]][1]-1].unlock();
//         }

//         // JUMP:
//     }

// }



void push_PIT_buffer(int tid){
    int i;
    while((i=TASK_ID++)<=NumOfNodes){
        for (int n=0; n<buf_itr[i]; ++n){
                NodeArr[i].PIT_buffer.push(BUF_pit_buffer[i][n]);
        }
    }
}

void Actual_Forwarding()
{
    Interest_packet temp;
    int visited[NumofObj + 1];
    // for (int i=1;i<=NumOfNodes;++i) buf_itr[i] = 0;

    for(int i = NumOfNodes; i >=1; --i){
        memset(visited,0,sizeof(visited));
        while(!NodeArr[i].PIT.empty()){
            temp = NodeArr[i].PIT.front();
            NodeArr[i].PIT.pop();
            if(visited[temp.content])
                continue;
            visited[temp.content] = 1;
            Actual_Forwarding_packet(i,temp);
        }
    }


    // TASK_ID = 1;
    // for (int i=0;i<THREAD_NUM;++i){
    //     thd[i] = thread(push_PIT_buffer,i);
    // }
    // // printf("backcnt = %d\n", (int)backcnt);
    //  for (int i=0;i<THREAD_NUM;++i){
    //     thd[i].join();
    // }
}

void initial_state(int tid){
   int i;

    while((i=TASK_ID++)<=NumOfNodes){
        NodeArr[i].Clear_FIB_Buffer();
        for(int k = 1; k <= NumofObj; ++k){
            NodeArr[i].data_que[k].acc_buffer = NodeArr[i].data_que[k].acc;
        }
    }
}
void Initial_State()
{
    memset(mu_a_b_k,0,sizeof(mu_a_b_k));
    memset(v_a_n_k,0,sizeof(v_a_n_k));
    TASK_ID = 1;
    #ifdef MULTITHREAD
    for (int i=0;i<THREAD_NUM;++i){
        thd[i] = thread(initial_state,i);
    }
     for (int i=0;i<THREAD_NUM;++i){
        thd[i].join();
    }
    #else
    initial_state(0);
    #endif

}

void release_qsi(int tid){
    int i;

    while((i=TASK_ID++)<=NumOfNodes){
        for(int k = 1; k <= NumofObj; ++k)
            NodeArr[i].data_que[k].acc = NodeArr[i].data_que[k].acc_buffer;
    }
}
void Release_QSI()
{
    TASK_ID = 1;
    #ifdef MULTITHREAD
    for (int i=0;i<THREAD_NUM;++i){
        thd[i] = thread(release_qsi,i);
    }
     for (int i=0;i<THREAD_NUM;++i){
        thd[i].join();
    }
    #else
    release_qsi(0);
    #endif
}
/*========================== Main Function =========================*
            *** supposing first forward then backup***
====================================================================*/

int main()
{
    srand(time(NULL));
    cnt1 = cnt2 = 0;
    const time_t srt = time(NULL);
    unsigned int seed = (unsigned long long)time(NULL);

    read_file = fopen("./data_3000_obj_0.75.txt","r+");


    Initalize_Topology();
    Init_Distance();

    /*init*/
    Trans_Remain = 0;
    delay = 0;
    hit = 0;
    has_finished = 0;
    Files = 0;
    signal_p = 0;




    //memset(clients,150,sizeof(clients));
    for(int i = 1; i <= Total_Time * NumOfNodes + 5;++i)
        clients[i] = 60;

    ratio_z = 0,delta = 0;
    W = 0.05;

    double QSI = 0;


    for(int m = 0; m < Total_Time; ++m){
        Cur_Time += 1;
        printf("%d\n",Cur_Time );

        memset(A_n_k,0,sizeof(A_n_k));
        memset(Total_Request,0,sizeof(Total_Request));

        for(int i = 1; i <= NumOfNodes; ++i)
            Read_File(i,clients[Cur_Turn++]);

        for(int i = 1; i <= NumOfNodes; ++i)
            for(int k = 0; Total_Request[i][k]; ++k)
                A_n_k[i][Total_Request[i][k]] += scale;

       // Congestion_Control();
        Initial_State();
        Virtual_Forwarding();

        if(signal_p > File_Max - NumOfNodes*clients[1]){
            for(int m = 1; m <= NumOfNodes; ++m)
                NodeArr[m].clear_signal();
            signal_p = 0;
        }

        Actual_Request();
        Actual_Forwarding();
        Actual_Back();
        Update_Vt();
        Release_QSI();
        Release_Buffer();



        int ss = 0;
        for(int k = 1; k <= NumOfNodes; ++k)
            for(int mm = 1; mm <= NumofObj; ++mm)
            {
                QSI += NodeArr[k].data_que[mm].acc;
                ss += NodeArr[k].time_record[mm][1];
            }

        if( Min_Time < Cur_Time)
            break;

         printf("Remain Obj: %d\n",ss );
         printf("Trans Remain: %d\n", (int)Trans_Remain);
        // cout << "QSI" << QSI << endl;
    }

    for(int i = 1; i <= NumOfNodes; ++i)
        for(int k = 1; k <= NumofObj; ++k){
            if(sum_a_n_k[i][k]){
                Total_Utility += (-1.0/(sum_a_n_k[i][k]));
            }
        }

    cout << "unchange: " << unchange << endl;
    cout << "change: " << change << endl;
    cout << "Total Utility: " << Total_Utility << endl;


    int ss = 0;

    for(int k = 1; k <= NumOfNodes; ++k)
        for(int mm = 1; mm <= NumofObj; ++mm){
                ss += NodeArr[k].time_record[mm][1];
    }

    const time_t finish = time(0);

    cout << "Simulation Time Cost: " << (finish-srt)<<" s" << endl;



    cout << "VIP remain request: " << ss << endl;

    cout << cnt1 << "  " << cnt2 << endl;

    cout << "--------------VIP----------Original-------" << endl;
    cout << "SHRINK_RATIO : " << SHRINK_RATIO <<endl;
    cout << "z: " <<ratio_z << "  delta: " << delta << endl; 
    cout << "hit : " << hit << endl;
    cout << "delay : " << delay << endl;
    cout << "has finished : " << has_finished << endl;
    cout << "average hit: " << double(hit) / has_finished << endl;
    cout << "average delay: " << double(delay) / has_finished << endl;
    cout << "Total Files :" << Files << endl;
    cout << endl;
    ofstream fout;
    fout.open("./data_10_new.txt");

    fout << "VIP QSI: " << QSI << endl;
    fout << "Total utility: " << Total_Utility << endl;
    fout << "--------------VIP----------z: " << ratio_z << " W:"<< W << endl;
    fout << "hit : " << hit << endl;
    fout << "delay : " << delay << endl;
    fout << "has finished : " << has_finished << endl;
    fout << "average hit: " << double(hit) / has_finished << endl;
    fout << "average delay: " << double(delay) / has_finished << endl;
    fout << "Total Files :" << Files << endl;
    fout << endl;

    double tmp;
    
    for(int i = 1; i <= NumOfNodes; ++i){
        fout << "Node :" << i << endl;
        tmp = 0;
        for(int k = 1; k <= NumofObj; ++k){
            fout << NodeArr[i].data_que[k].acc<< ' ';
            tmp += NodeArr[i].data_que[k].acc;
        }
        fout << endl;
        fout << "Aver:" << tmp/NumofObj <<endl;
    }

    // for (int k=1;k<=NumofObj;++k){
    //     fout << "OBJ " << k << ": "; 
    //     for (int i=1; i<=NumOfNodes;++i){
    //         fout << NodeArr[i].CS[k] << ' ';
    //     }
    //     fout<<endl;
    // }

    return 0;
}

