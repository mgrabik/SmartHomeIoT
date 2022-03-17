#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <regex.h>

#define SERWER	"192.168.126.4"
#define MAX_BUF	1200
#define FILE_NAME	"rules.txt"

enum Operator{Null, Greater, Less, Equal}; //Greater = ">", Less = "<", Equal = "=="
enum Connector{Null1, And, Or}; // And = "&&", Or = "||"

struct args {
	struct sockaddr_in deviceIP;
	char *actionToSend;
	int waitingTime;
};

struct THENRule {
	int rule_id;
    char *location;
    char *object;
    char *action;
};

struct IFRule {
	int rule_id;
    char *location[5];
    char *object[5];
    char *state[10];
    char *quantity[5];
    enum Operator operators[5];
    int limitValue[5];
    enum Connector connectors[5];
    struct THENRule thenRule;
};

struct Device {
	int device_id;
	char timestamp[50];
	char location[50];
	char object[50];
	char quantity[10][50];
	char action[20][50];
	char state[20][50];
	struct sockaddr_in deviceAddr;
};

// Global variables
struct addrinfo h, *r;
struct sockaddr_in nodeAddr;
unsigned char message[MAX_BUF];
int s, addr_len=sizeof(nodeAddr);
const char *messageType[] = {"ENROLLING_NEW_DEVICE", "PROVIDING_DATA"};
char * ifRuleLine[20];
char * thenRuleLine[20];
struct IFRule allRules[20];
struct Device allDevices[20];
char *tempToken;
char *ifData[50];
int gindex = 0, i = 0, b = 0, ruleIndex = 0, numberOfDevices = 0, IdOfPacketsSent = 1;
int isConnector[50], tempRule[30], tempDevice[30];
struct IFRule ifRuleTemp;
struct THENRule thenRuleTemp;
struct args input;

// Method remove identical elements from the array
void DeleteDuplicate(int array[], int size){
	for(int i = 0; i < size;i++){
		for(int j = i + 1; j < size;j++){
			if(array[i] == array[j]){
				for(int k = j; k < size - 1; k++){
					array[k] = array[k+1];
				}
				size--;
				j--;
			}
		}
	}
}
/*
	Method looks for a substring in a given string
	Returns 1 when match was found
	Returns 0 when match was not found
*/
int My_regex(char string[], char wanted[]){
    regex_t regex;
    int reti = regcomp(&regex, wanted, 0);
    if (reti) {
        fprintf(stderr, "Could not compile regex\n");
        exit(-1);
    }
    reti = regexec(&regex, string, 0, NULL, 0);
    regfree(&regex);
    if (!reti) {
        return 1; // Match was found
    }
    else if (reti == REG_NOMATCH) {
        return 0;// Match was not found
    }
    else {
        printf("ERROR in method 'My_Regex'\n");
        exit(-1); // ERROR
    }
}

// Method removes the space from the last character, if any.
char* DeleteLastSpace(char *string){
    if(string[strlen(string) - 1] == ' '){
        string[strlen(string) - 1] = '\0';
    }
    return string;
}

// Method reads quantities and saves valeus to the structure
void getValuesFromQuantity(char *line, int i){
	// Checking operator
    if(My_regex(line, ">")){
        ifRuleTemp.operators[i] = Greater;
    }else if(My_regex(line, "<")){
        ifRuleTemp.operators[i] = Less;
    }else if(My_regex(line, "=")){
        ifRuleTemp.operators[i] = Equal;
    }else{
        printf("Error in Rule File!\n");
        exit(-1);
    }
    
    ifRuleTemp.quantity[i] = strtok(line, "><=");
    ifRuleTemp.quantity[i] = DeleteLastSpace(ifRuleTemp.quantity[i]);
    
    tempToken = strtok(NULL, ",");
    if(tempToken[0] == ' '){
        memmove(tempToken, tempToken+1, strlen(tempToken));
    }
	// Setting the value to the rule's structure
    ifRuleTemp.limitValue[i] = atoi(DeleteLastSpace(tempToken));
    
}

// Method reads time and saves valeus to the structure
void getValuesFromTime(char *line, int i){
	// Checking operator
    if(My_regex(line, ">")){
        ifRuleTemp.operators[i] = Greater;
    }else if(My_regex(line, "<")){
        ifRuleTemp.operators[i] = Less;
    }else if(My_regex(line, "=")){
        ifRuleTemp.operators[i] = Equal;
    }else{
        printf("Error in Rule File!\n");
        exit(-1);
    }
    ifRuleTemp.quantity[i] = "time";
	
	// Checking if the given time value is in seconds or minutes
    if(line[strlen(line) - 3] == 's'){
        tempToken = strtok(line, "<>=");
        tempToken = strtok(NULL, "s");
        if(tempToken[0] == ' ') memmove(tempToken, tempToken+1, strlen(tempToken));
        ifRuleTemp.limitValue[i] = atoi(DeleteLastSpace(tempToken));
    }else if(line[strlen(line) - 3] == 'm'){
        tempToken = strtok(line, "<>=");
        tempToken = strtok(NULL, "s");
        if(tempToken[0] == ' ') memmove(tempToken, tempToken+1, strlen(tempToken));
        ifRuleTemp.limitValue[i] = atoi(DeleteLastSpace(tempToken)) * 60;
    }else{
        printf("Error in Rule File!\n");
        exit(-1);
    }
}

// Method reads IfRule from the rules file
void ReaderIf(char *line[], int i){
    int j = 0;
    char *tokens[3];
    char *token = strtok(line[gindex - 1], ",");
    tokens[j++] = token;
	
    while(j < 3){
        token = strtok(NULL, ",");
        tokens[j++] = token;
    }

	//Remove first space, if any
    if(tokens[0][0] == ' ') memmove(tokens[0], tokens[0]+1, strlen(tokens[0]));
	/*
		Saveing rule's data to the temp object 'ifRuleTemp'
		First variable is 'location' or 'time'
	*/
    if(tokens[0][0] == 'l'){
        ifRuleTemp.location[i] = tokens[0];
    }else if(tokens[0][0] == 't'){
        tokens[0] = DeleteLastSpace(tokens[0]);
        getValuesFromTime(tokens[0], i);
        goto jump;
        return;
    }else{
        printf("Error in Rule File!\n");
        exit(-1);
    }
    
    // Second variable -> 'object'
    if(tokens[1][0] == ' ') memmove(tokens[1], tokens[1]+1, strlen(tokens[1]));
    ifRuleTemp.object[i] = tokens[1];
    
    if(tokens[2][0] == ' ') memmove(tokens[2], tokens[2]+1, strlen(tokens[2]));
    tokens[2] = DeleteLastSpace(tokens[2]);
	
    // Third variable -> 'quantity' or 'state'
    if(tokens[2][0] == 'q'){
        getValuesFromQuantity(tokens[2], i);
    }else if(tokens[2][0] == 's'){
        ifRuleTemp.state[i] = tokens[2];
    }else{
        printf("Error in Rule File!\n");
        exit(-1);
    }
    
	// Looking for connectors: 'And' or 'Or'
jump:
    switch(isConnector[b++]){
        case 0:
            return;
        case 1:// &&
            ifRuleTemp.connectors[i++] = And;
            gindex++;
            ReaderIf(line, i); // Recall method 'ReaderIf', because there is more conditions in this rule
            break;
        case 2:// ||
            ifRuleTemp.connectors[i++] = Or;
            gindex++;
            ReaderIf(line, i); // Recall method 'ReaderIf', because there is more conditions in this rule
            break;
    }
}
// Method reads ThenRule from the rules file
void ReaderThen(char *line){
    int j = 0;
    char *tokens[3];
    char *token = strtok(line, ",");
    tokens[j++] = token;
    while(j < 3){
        token = strtok(NULL, ",");
        tokens[j++] = token;
    }
    
    /*
		Saveing rule's data to the temp object 'ifRuleTemp'
		First variable is 'location'
	*/
    if(tokens[0][0] == ' ') tokens[0]+1;
    
    if(tokens[0][0] == 'l'){
        thenRuleTemp.location = tokens[0];
    }else{
        printf("Error in Rule File!\n");
        exit(-1);
    }
    
    // Second variable -> 'object'
    if(tokens[1][0] == ' ') memmove(tokens[1], tokens[1]+1, strlen(tokens[1]));
    thenRuleTemp.object = tokens[1];
    
    // Third variable -> 'action'
    if(tokens[2][0] == ' ') memmove(tokens[2], tokens[2]+1, strlen(tokens[2]));
    tokens[2] = DeleteLastSpace(tokens[2]);
    
    if(tokens[2][0] == 'a'){
        thenRuleTemp.action = tokens[2];
    }else{
        printf("Error in Rule File!\n");
        exit(-1);
    }
}
// Method saves each new device in the structure 'Device'
void saveNewDevide(unsigned char message[]){
	struct Device newDevice;
	int j = 0, k = 0;
	char *tokens[30];
	char * token = strtok(message, ";");
	while (token != NULL){
		tokens[j++] = token;
		token = strtok(NULL, ";");
	}
	k = 0;
	newDevice.device_id = atoi(tokens[1]);
	while(tokens[2][k] != '\0'){
		newDevice.timestamp[k] = tokens[2][k];
		k++;
	}
	k = 0;
	while(tokens[3][k] != '\0'){
		newDevice.location[k] = tokens[3][k];
		k++;
	}
	k = 0;
	while(tokens[4][k] != '\0'){
		newDevice.object[k] = tokens[4][k];
		k++;
	}
	
	j = 0, k = 0;
	token = strtok(tokens[5], ",");
	while (token != NULL){
		while(token[k] != '\0'){
			newDevice.quantity[j][k] = token[k];
			k++;
		}
		token = strtok(NULL, ",");
		j++;
		k = 0;
	}
	j = 0, k = 0;
	token = strtok(tokens[6], ",");
	while (token != NULL){
		while(token[k] != '\0'){
			newDevice.action[j][k] = token[k];
			k++;
		}
		token = strtok(NULL, ",");
		j++;
		k = 0;
	}
	j = 0, k = 0;
	token = strtok(tokens[7], ",");
	while (token != NULL){
		while(token[k] != '\0'){
			newDevice.state[j][k] = token[k];
			k++;
		}
		token = strtok(NULL, ",");
		j++;
		k = 0;
	}
	
	allDevices[numberOfDevices++] = newDevice;
	
	memset(&newDevice, 0, sizeof(struct Device));
}
/*
	Method saves rule's IDs into the array where given 'quantity' occurs
	Returns the last found ID
*/
int getAppropriateRuleByQuantity(char *quantity){
	memset(tempRule, 0, sizeof(tempRule));
	int isError = 1;
	for(int i = 0; i < 20; i++){
		for(int j = 0; j < 5; j++){
			if(allRules[i].quantity[j] != NULL && strncmp(allRules[i].quantity[j], quantity, strlen(quantity)) == 0){
				tempRule[gindex++] = i;
				isError = 0;
				continue;
			}
		}
	}
	if(isError){
		return -1;
	}
	return gindex;
}
/*
	Method saves rule's IDs into the array where given 'state' occurs
	Returns the last found ID
*/
int getAppropriateRuleByState(char *state){
	memset(tempRule, 0, sizeof(tempRule));
	int isError = 1;
	for(int i = 0; i < 20; i++){
		for(int j = 0; j < 10; j++){
			if(allRules[i].state[j] != NULL && strncmp(allRules[i].state[j], state, strlen(state)) == 0){
				tempRule[gindex++] = i;
				isError = 0;
				continue;
			}
		}
	}
	if(isError){
		return -1;
	}
	return gindex;
}
//Method returns appropriate device with given 'action'
int getAppropriateDevice(char *action){
	memset(tempDevice, 0, sizeof(tempDevice));
	for(int i = 0; i < 20; i++){
		for(int j = 0; j < 20; j++){
			if(allDevices[i].action[j] != NULL && strncmp(allDevices[i].action[j], action, strlen(action)) == 0){
				return i;
			}
		}
	}
	return -1;
}
// Method counts connectors in a given rule and returns the value
int countConnectors(struct IFRule rule){
	int result = 0;
	for(int i = 0;i < 5;i++){
		if(rule.connectors[i] != Null1){
			result++;
		}
	}
	return result;
}
// Method returns limitValue of time from a given rule
int getLimitValueOfTime(struct IFRule rule){
	char *template = "time";
	for(int i = 0;i < 5;i++){
		if(rule.quantity[i] != NULL && strncmp(rule.quantity[i], template, strlen(template))== 0){
			return rule.limitValue[i];
		}
	}
	return 0;
}
// Method sends commands to the nodes
void* sendCommand( void *tempArgs){
	struct args *input3 = (struct args*)tempArgs;
	struct args input1 = *input3;
	memset(&input, 0, sizeof(struct args));
	char id[10];
	sprintf(id, "%d", IdOfPacketsSent++);
	unsigned char commandString[300];
	struct sockaddr_in destIP =  input1.deviceIP;
	int wait = input1.waitingTime;
	if(wait){
		printf("Serwer czeka z wyslaniem wiadomosci %d sekund\n", wait);
		sleep(wait);
		printf("Oczekiwanie minelo i serwer wysyla wiadomosc\n");
	}
	time_t rawtime;
    time( &rawtime );
	snprintf(commandString, 300, "COMMAND_DO_SOMETHING;\
%s;\
%s;\
%s;\
END;",id, asctime(localtime( &rawtime )), input1.actionToSend);
	
	int pos = sendto(s, commandString, strlen(commandString), 0, (struct sockaddr *)&destIP, sizeof(destIP));
	if(pos<0){
		printf("ERROR: %s\n", strerror(errno));
		exit(-4);
	}else{
		printf("Wyslano polecenie do urządzenia!\n");
	}
	return NULL;
}
// Method gets all informations from the Nodes and reads them
void checkProvidedData(){
	int j = 0, isQuantity = 1;
	int q, s;
	char *tokens[30];
	char * token = strtok(message, ";");
	while (token != NULL){
		if(token[0] == 'q'){
			q = j;
		}else if(token[0] == 's'){
			s = j;
		}
		tokens[j++] = token;
		token = strtok(NULL, ";");
	}
	j = 0;
	char *empty = "   ";
	char *quantities[30] = {0};
	char *values[30] = {0};
	char *states[15] = {0};
	
	token = strtok(tokens[q], ",");
	while (token != NULL){
		if(isQuantity){
			quantities[j] = token;
			token = strtok(NULL, ",");
			isQuantity = 0;
		}else{
			values[j++] = token;
			printf("%s ma wartość %s\n", quantities[j-1]+1, values[j-1]);
			token = strtok(NULL, ",");
			isQuantity = 1;
		}
	}
	for(j; j < 30; j++){
		quantities[j] = empty;
	}
	j = 0;
	token = strtok(tokens[s], ",");
	while (token != NULL){
		states[j++] = token;
		token = strtok(NULL, ",");
	}
	
	j = 0, gindex = 0;
	int i1 = 0, i2 = 0, isError, searchedId;
	int ruleIds[30] = {0};
	int deviceIds[30] = {0};
	int noQuantity[15] = {0};
	int noState[15] = {0};
	
	while(quantities[j] != empty){
		isError = getAppropriateRuleByQuantity(quantities[j]);
		if(isError != -1){
			i1 = i1 + isError;
			memcpy(ruleIds + (i1 - isError), tempRule, isError*sizeof(int));
			gindex = 0;
		}else{
			noQuantity[j] = 1;
		}
		j++;
	}
	j = 0, gindex = 0;
	while(states[j] != NULL){
		isError = getAppropriateRuleByState(states[j]);
		if(isError != -1){
			//i1 = i1 + isError;
			//memcpy(ruleIds + (i1 - isError), tempRule, isError*sizeof(int));
			//gindex = 0;
		}else{
			noState[j] = 1;
		}
		j++;
	}
	DeleteDuplicate(ruleIds, sizeof(ruleIds) / sizeof(ruleIds[0]));
	for(int i = 0; i < i1; i++){
		searchedId = getAppropriateDevice(allRules[ruleIds[i]].thenRule.action);
		if(searchedId != -1){
			deviceIds[ruleIds[i]] = searchedId;
		}else{
			printf("Nie znaleziono urzadzenia spelniajacego warunki!\n");
			exit(-1);
		}
	}
	
	int AmountOfConnectors[50] = {0};
	for(int i = 0; i < i1; i++){
		AmountOfConnectors[ruleIds[i]] = countConnectors(allRules[ruleIds[i]]);
	}
	int isTimer[50] = {0};
	for(int i = 0; i < i1; i++){
		isTimer[ruleIds[i]]= getLimitValueOfTime(allRules[ruleIds[i]]);
	}
	int x = 0, ri;
	pthread_t newthread1;
	for(int i = 0; i < i1; i++){
		// Searching by quantity
		ri = ruleIds[i];
		for(j = 0; j < 5; j++){
			x = 0;
			if(allRules[ri].quantity[j] != NULL){
				while(noQuantity[x] == 0 && quantities[x] != empty){
					if(strncmp(allRules[ri].quantity[j], quantities[x], strlen(allRules[ri].quantity[j])) == 0){
						if(allRules[ri].operators[j] == Greater){
							if(allRules[ri].limitValue[j] < atoi(values[x])){
								if(AmountOfConnectors[ri]){
									AmountOfConnectors[ri]--;
									break;
								}else{
									if(isTimer[ri]){
										input.waitingTime = isTimer[ri];
									}
									input.deviceIP = allDevices[deviceIds[ri]].deviceAddr;
									input.actionToSend = allRules[ri].thenRule.action;
									struct args *input2 = &input;
									pthread_create(&newthread1, NULL, sendCommand, (void* )input2);
								}
							}else{
								j = 5;
							}
						}else if(allRules[ri].operators[j] == Less){
							if(allRules[ri].limitValue[j] > atoi(values[x])){
								if(AmountOfConnectors[ri]){
									AmountOfConnectors[ri]--;
									break;
								}else{								
									if(isTimer[ri]){
										input.waitingTime = isTimer[ri];
									}
									input.deviceIP = allDevices[deviceIds[ri]].deviceAddr;
									input.actionToSend = allRules[ri].thenRule.action;
									struct args *input2 = &input;
									pthread_create(&newthread1, NULL, sendCommand, (void* )input2);
								}
							}else{
								j = 5;
							}
						}else{
							if(allRules[ri].limitValue[j] == atoi(values[x])){
								if(AmountOfConnectors[ri]){
									AmountOfConnectors[ri]--;
									break;
								}else{
									if(isTimer[ri]){
										input.waitingTime = isTimer[ri];
									}
									input.deviceIP = allDevices[deviceIds[ri]].deviceAddr;
									input.actionToSend = allRules[ri].thenRule.action;
									struct args *input2 = &input;
									pthread_create(&newthread1, NULL, sendCommand, (void* )input2);
								}
							}else{
								j = 5;
							}
						}
					}else if(strncmp(allRules[ri].quantity[j], "time", strlen(allRules[ri].quantity[j])) == 0){
						if(AmountOfConnectors[ri]){
							AmountOfConnectors[ri]--;
							break;
						}else{
							if(isTimer[ri]){
								input.waitingTime = isTimer[ri];
							}
							input.deviceIP = allDevices[deviceIds[ri]].deviceAddr;
							input.actionToSend = allRules[ri].thenRule.action;
							struct args *input2 = &input;
							pthread_create(&newthread1, NULL, sendCommand, (void* )input2);
						}
					}
					x++;
				}
			}
		}
		
		// Searching by State
		for(j = 0; j < 10; j++){
			x = 0;
			if(allRules[ri].state[j] != NULL){
				while(noState[x] == 0 && states[x] != NULL){
					if(strncmp(allRules[ri].state[j], states[x], strlen(allRules[ri].state[j])) == 0){
						if(AmountOfConnectors[ri]){
							AmountOfConnectors[ri]--;
						}else{
							if(isTimer[ri]){
								input.waitingTime = isTimer[ri];
							}
							input.deviceIP = allDevices[deviceIds[ri]].deviceAddr;
							input.actionToSend = allRules[ri].thenRule.action;
							struct args *input2 = &input;
							pthread_create(&newthread1, NULL, sendCommand, (void* )input2);
						}
					}
					x++;
				}
			}
		}
	}
}
// Method reads all rules from the 'rules.txt' and saves data in the array of structures 'AllRules'
void ReadingRules(){
	int index = 0, IFflag = 1, g = 0, p = 0;
	FILE * file;
	file = fopen(FILE_NAME, "r");
	if(file == NULL){
		printf("Error: Nie mozna otworzyc pliku %s", FILE_NAME);
		exit(-4);
	}
	const unsigned MAX_LENGTH = 200;
	char buffer[MAX_LENGTH];
	char *token;
	char lines[50][200];
	
	while(fgets(buffer, MAX_LENGTH, file)){
		strcpy(lines[index], buffer);
		index++;
	}
	fclose(file);
	index = 0;
	while(lines[g][0] != '\0'){
		token = strtok(lines[g++], "(");
		token = strtok(NULL, ")");
		if(!IFflag){
			thenRuleLine[index++] = token;
			IFflag = 1;
		}else{
			ifRuleLine[index] = token;
			IFflag = 0;
		}
	}
	g = 0;
	// Counting connectors '&&' and '||'
	while(ifRuleLine[g] != NULL){
		char *fChar = strpbrk(ifRuleLine[g], "&|");
		while(fChar != NULL){
			if(fChar != NULL && *fChar == '&'){
				isConnector[g+b++] = 1;
			}else if(fChar != NULL && *fChar == '|'){
				isConnector[g+b++] = 2;
			}
			fChar = fChar+2;
			fChar = strpbrk(fChar, "&|");
		}
		// Split lines by '&' or '|'
		char *tempifData = strtok(ifRuleLine[g], "&|");
		while(tempifData != NULL){
			ifData[p++] = tempifData;
			tempifData = strtok(NULL, "&|");
		}
		g++;
	}
	
	g = 0, b = 0;
	// Save the rule in the array 'allRules'
	while(ifData[gindex++] != NULL){
		i = 0;
		ifRuleTemp.rule_id = gindex;
		ReaderIf(ifData, i);
		allRules[ruleIndex++] = ifRuleTemp;
		
		// Clearing the memory
		memset(&ifRuleTemp, 0, sizeof(&ifRuleTemp));
		memset(&ifRuleTemp.location, 0, 5*sizeof(&ifRuleTemp.location));
		memset(&ifRuleTemp.object, 0, 5*sizeof(&ifRuleTemp.object));
		memset(&ifRuleTemp.state, 0, 10*sizeof(&ifRuleTemp.state));
		memset(&ifRuleTemp.quantity, 0, 5*sizeof(&ifRuleTemp.quantity));
		memset(&ifRuleTemp.operators, 0, 5*sizeof(&ifRuleTemp.operators));
		memset(&ifRuleTemp.limitValue, 0, 5*sizeof(&ifRuleTemp.limitValue));
		memset(&ifRuleTemp.connectors, 0, 5*sizeof(&ifRuleTemp.connectors));
		

	}
	gindex = 0, ruleIndex = 0;
	// Add to existing rules actions --> ThenRule
	while(thenRuleLine[gindex++] != NULL){
		thenRuleTemp.rule_id = gindex;
		ReaderThen(thenRuleLine[gindex - 1]);
		
		allRules[ruleIndex++].thenRule = thenRuleTemp;
		
		// Clearing the memory
		memset(&thenRuleTemp, 0, sizeof(&thenRuleTemp));
		memset(&thenRuleTemp.location, 0, sizeof(&thenRuleTemp.location));
		memset(&thenRuleTemp.object, 0, sizeof(&thenRuleTemp.object));
		memset(&thenRuleTemp.action, 0, sizeof(&thenRuleTemp.action));
	}
	
	printf( "Reguły zostały wczytane!\n");
}

int main(){
	memset(&h, 0, sizeof(struct addrinfo));
	h.ai_family=PF_INET;
	h.ai_socktype=SOCK_DGRAM;
	h.ai_flags=AI_PASSIVE;
	
	if(getaddrinfo(NULL, "13588", &h, &r)!=0){
		printf("ERROR during getaddrinfo!\n");
	}
	s = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
	if(s==-1){
		printf("ERROR: %s (%s:%d)\n", strerror(errno), __FILE__, __LINE__);
		exit(-1);
	}
	if(bind(s, r->ai_addr, r->ai_addrlen)!=0){
		printf("ERROR: %s (%s:%d)\n", strerror(errno),__FILE__, __LINE__);
		exit(-1);
	}
	
	ReadingRules();
	
	int result;

	printf("Nasłuchiwanie zgłoszeń...\n");
	for(;;){
		memset(&nodeAddr, 0, sizeof(nodeAddr));
		memset(message, 0, MAX_BUF);
		result = recvfrom(s, message, MAX_BUF, 0, (struct sockaddr*)&nodeAddr, &addr_len);
		if(result<0){
			printf("ERROR: %s\n", strerror(errno));
			exit(-4);
		}
		if(strncmp(message, messageType[0], strlen(messageType[0]) - 1) == 0){
			saveNewDevide(message);
			allDevices[numberOfDevices - 1].deviceAddr = nodeAddr;
			printf("Nowe urządzenie zostało zarejestrowane! (%s:%d)\n",inet_ntoa(nodeAddr.sin_addr), ntohs(nodeAddr.sin_port));
		}else if(strncmp(message, messageType[1], strlen(messageType[1]) - 1) == 0){
			printf("Przyszedł raport od urządzenia.\n");
			message[15] = ' ';
			checkProvidedData();
		}else{
			printf("Wiadomosc niepoprawna!\n");
		}
	}
}