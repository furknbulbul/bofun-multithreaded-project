#include <unistd.h>
#include <cstring>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

// struct of customer
typedef struct {
    int id;
    unsigned int sleep_time;
    int vending_machine_id;
    int payment_amount;
    string company;
}Customer ;


pthread_barrier_t barrier;
ofstream myoutput; // ofstream declaration
int company_balances[5]; // holding compnay balances
pthread_mutex_t common_mutex; // used for locking data that is possible to used by all threads
pthread_mutex_t balance_mutex[5]; // used for updating the balances of companues
pthread_mutex_t vending_machine_mutex[10]; // used when transferring data to machine threads
pthread_mutex_t customer_mutex[10]; // used for locking between customer threads that goes to the same machines
pthread_cond_t vending_machine_cond[10]; // condition array for each machine gets the signal of customer arrival
pthread_cond_t customer_cond[10]; // condition array for each machine gets the signal of customer arrival
int vending_machine_busy[10];// boolean for if a machine has a customer
Customer arrival_customers[10]; // customer arrived to the machine for transaction
int finished; // representing if customers are done

// splitting functions to get tokens
vector<string> split(const string& str, char delimiter) {
    vector<string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    while (end != string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(delimiter, start);
    }
    tokens.push_back(str.substr(start));
    return tokens;
}

// map company names to indexes
int get_company_index(string company_name){
    if(company_name=="Bob") return 0;
    else if (company_name == "Dave") return 1;
    else if (company_name == "Kevin") return 2;
    else if (company_name == "Otto") return 3;
    else if (company_name == "Stuart") return 4;
    return 0;
}
string companies[5] = {"Bob", "Dave","Kevin", "Otto", "Stuart"};

// return value of finished
int is_finished(){
    int tmp;
    pthread_mutex_lock(&common_mutex);
    tmp = finished;
    pthread_mutex_unlock(&common_mutex);
    return tmp;
}

// send finish signal to all waiting machine threads
void finish_all(){
    pthread_mutex_lock(&common_mutex);
    finished = 1;
    pthread_mutex_unlock(&common_mutex);
    for( int i = 0 ; i< 10; i++){
        pthread_cond_signal(&vending_machine_cond[i]);
    }
}

// runner function of vending machine thread
void* vending_machine_runner(void* param){
    int machine_id  = *((int*) param);

    while(true) {

        pthread_mutex_lock(&vending_machine_mutex[machine_id]);

        while (!vending_machine_busy[machine_id] && !is_finished()){
            pthread_cond_wait(&vending_machine_cond[machine_id], &vending_machine_mutex[machine_id]);
        }
        Customer arrival_customer = arrival_customers[machine_id];
        // to ensure that last customer is processed before exiting
        if(is_finished()&&arrival_customer.id ==-1) break;
        Customer tmp;
        tmp.id = -1;
        arrival_customers[machine_id] = tmp;
        pthread_mutex_unlock(&vending_machine_mutex[machine_id]);

        int company_index = get_company_index(arrival_customer.company);

        // to prevent machine threads do not print out at the same time
        pthread_mutex_lock(&common_mutex);
        myoutput<<"Customer"<< arrival_customer.id+1<<","<<arrival_customer.payment_amount<<"TL,"<<companies[company_index]<<endl;
        pthread_mutex_unlock(&common_mutex);

        // to prevent machine threads do not update the company balances at the same time

        pthread_mutex_lock(&balance_mutex[company_index]);
        company_balances[company_index] += arrival_customer.payment_amount;
        pthread_mutex_unlock(&balance_mutex[company_index]);

        // send signal to customer that is it not busy
        pthread_mutex_lock(&customer_mutex[machine_id]);
        vending_machine_busy[machine_id] = 0;

        pthread_cond_signal(&customer_cond[machine_id]);
        pthread_mutex_unlock(&customer_mutex[machine_id]);

       usleep(1000); // sleep 1ms so that other machine find chance to print its result

    }

    pthread_exit(nullptr);
}

// wait for machine to get available and send customer data to machine thread
void send_customer_to_machine(Customer customer){

    int machine_id  = customer.vending_machine_id;

    // lock mutex while waiting for signal coming from machine
    pthread_mutex_lock(&customer_mutex[machine_id]);
    while(vending_machine_busy[machine_id]){
        pthread_cond_wait(&customer_cond[machine_id], &customer_mutex[machine_id]);
    }
    pthread_mutex_unlock(&customer_mutex[machine_id]); // unlock this customer's mutex so that it can transfer data to mahine

    // lock mutex while sending data to machine
    pthread_mutex_lock(&vending_machine_mutex[machine_id]);
    arrival_customers[machine_id] = customer; // put data to shared adress between customers and machines
    vending_machine_busy[machine_id] = 1; // update the condition if machine in a wait state
    pthread_cond_signal(&vending_machine_cond[machine_id]); // signal to waiting machine
    pthread_mutex_unlock(&vending_machine_mutex[machine_id]); // unlock machine mutex after sending data to machine

}


// custeomer runner thread
void* customer_runner(void* param){
    Customer customer = *((Customer*) param);
    pthread_barrier_wait(&barrier);
    usleep(customer.sleep_time); // sleep for waited time
    send_customer_to_machine(customer);
    return nullptr;

}

int main(int argc, char* argv[]) {

    // handle file
    string input_file_name = argv[1];
    ifstream myfile;
    myfile.open(input_file_name);
    string temp = split(input_file_name, '.')[0];
    string output_file_name  = temp + "_log.txt";
    myoutput = ofstream(output_file_name);
    string line;
    getline(myfile, line);
    int customer_number = stoi(line);

    // hold machine thread ids and init attr
    pthread_t vending_machine_tids[10];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_barrier_init(&barrier, nullptr, customer_number);

    // initialization
    for (int i = 0; i< 10; i++){
        Customer tmp;
        tmp.id =-1;
        arrival_customers[i]= tmp;
    }

    // init mutexes and conds
    for (int i =0; i < 5; i++){
        pthread_mutex_init(&balance_mutex[i], nullptr);
    }
    pthread_mutex_init(&common_mutex, nullptr);
    for (int i = 0 ; i< 10; i++) {
        pthread_mutex_init(&vending_machine_mutex[i], nullptr);
        pthread_cond_init(&vending_machine_cond[i], nullptr);
        pthread_cond_init(&customer_cond[i], nullptr);
        pthread_mutex_init(&customer_mutex[i], nullptr);

    }
    // create machine threads
    int arr[10];
    memset(arr, 0, sizeof(arr));
    for (int i = 0 ; i< 10; ++i) {
        arr[i]= i;
        pthread_create(&vending_machine_tids[i], &attr, vending_machine_runner, (void *) &arr[i]);

    }

    // create customer threads
    pthread_t customer_tids[customer_number];
    Customer* customer_pointers[customer_number];
    for(int i =0; i< customer_number; i++){
        vector<string> tokens;
        getline(myfile, line);
        tokens = split(line,  ',');

        int sleep_time  = stoi( tokens[0]);
        int vending_machine_id = stoi(tokens[1])- 1;
        string company_name = tokens[2];
        int payment_amount = stoi(tokens[3]);

        Customer* customer = (Customer*) malloc(sizeof (Customer));
        customer->id = i;
        customer->sleep_time = sleep_time*1000;
        customer->company = company_name;
        customer->payment_amount = payment_amount;
        customer->vending_machine_id = vending_machine_id;
        customer_pointers[i] = customer;
        pthread_create(&customer_tids[i], &attr, customer_runner, (void*)customer);

    }
    // wait for the customers to join and free customer pointers
    for(int i = 0 ; i <customer_number; i++){
        pthread_join(customer_tids[i], nullptr);
        free(customer_pointers[i]);
    }

    usleep(1000); //even though I think I handle the situation that machine thread is exited before the last customer finish, just be sure waiting 1ms
    finish_all();
    for (int i=0; i< 10; i++){
        pthread_join(vending_machine_tids[i], nullptr);
    }
    myoutput<<"All prepayments are completed.\n";

    myoutput<< "Kevin: " << company_balances[2]<<"TL"<<endl;
    myoutput<< "Bob: " << company_balances[0] << "TL"<<endl;
    myoutput<< "Stuart: " << company_balances[4] << "TL"<<endl;
    myoutput<< "Otto: " << company_balances[3]<< "TL"<<endl;
    myoutput<< "Dave: " << company_balances[1]<< "TL"<<endl;


    for (int i=0; i< 10; i++){
        pthread_mutex_destroy(&vending_machine_mutex[i]);
        pthread_cond_destroy(&vending_machine_cond[i]);
        pthread_mutex_destroy(&customer_mutex[i]);
        pthread_cond_destroy(&customer_cond[i]);
    }
    for (int i=0; i< 5; i++){
        pthread_mutex_destroy(&balance_mutex[i]);
    }
    pthread_mutex_destroy(&common_mutex);
    pthread_barrier_destroy(&barrier);

    myfile.close();
    myoutput.close();

    return 0;
}

