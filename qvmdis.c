/*
 * Q3VM Disassembler for Urban Terror by strata
 * What you do with this is not my responsibility.
 *
 * copyright (C) 2012 strata@dropswitch.net
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define NUM_BYTECODES 60
#define QAGAME         0
#define CGAME          1
#define UI             2

int qvmtype;

typedef struct {
	char *opcode;
	int param_size;
} q3vm_bytecode_t;

// Q3VM Bytecodes (http://icculus.org/~phaethon/q3vm_specs.html)
q3vm_bytecode_t bytecodes[NUM_BYTECODES] =
{
	{ "UNDEF ", 0 }, { "IGNORE", 0 }, { "BREAK ", 0 }, { "ENTER ", 4 },
	{ "LEAVE ", 4 }, { "CALL  ", 0 }, { "PUSH  ", 0 }, { "POP   ", 0 },
	{ "CONST ", 4 }, { "LOCAL ", 4 }, { "JUMP  ", 0 }, { "EQ    ", 4 },
	{ "NE    ", 4 }, { "LTI   ", 4 }, { "LEI   ", 4 }, { "GTI   ", 4 },
	{ "GEI   ", 4 }, { "LTU   ", 4 }, { "LEU   ", 4 }, { "GTU   ", 4 },
	{ "GEU   ", 4 }, { "EQF   ", 4 }, { "NEF   ", 4 }, { "LTF   ", 4 },
	{ "LEF   ", 4 }, { "GTF   ", 4 }, { "GEF   ", 4 }, { "LOAD1 ", 0 },
	{ "LOAD2 ", 0 }, { "LOAD4 ", 0 }, { "STORE1", 0 }, { "STORE2", 0 },
	{ "STORE4", 0 }, { "ARG   ", 1 }, { "BLOCK_COPY", 4 }, // NOT 0!
	{ "SEX8  ", 0 }, { "SEX16 ", 0 }, { "NEGI  ", 0 }, { "ADD   ", 0 },
	{ "SUB   ", 0 }, { "DIVI  ", 0 }, { "DIVU  ", 0 }, { "MODI  ", 0 },
	{ "MODU  ", 0 }, { "MULI  ", 0 }, { "MULU  ", 0 }, { "BAND  ", 0 },
	{ "BOR   ", 0 }, { "BXOR  ", 0 }, { "BCOM  ", 0 }, { "LSH   ", 0 },
	{ "RSHI  ", 0 }, { "RSHU  ", 0 }, { "NEGF  ", 0 }, { "ADDF  ", 0 },
	{ "SUBF  ", 0 }, { "DIVF  ", 0 }, { "MULF  ", 0 }, { "CVIF  ", 0 },
	{ "CVFI  ", 0 }
};

// Q3VM header
typedef struct {
	u_long magic;

	u_long instructionCount;

	u_long codeOffset;
	u_long codeLength;

	u_long dataOffset;
	u_long dataLength;

	u_long litLength;
	u_long bssLength;

	u_long jtrgLength;
} q3vm_header_t;

// Q3VM instruction
typedef struct {
	u_long offset;
	u_char bytecode;
	u_long param;
} instructionPointer_t;

// Functions
typedef struct {
	u_long startOffset;
	u_long endOffset;

	u_long startInstruction;
	u_long endInstruction;

	int frameSize;

	int byteSize;
	int instructionCount;

	u_long *calling;
	int n_calling;

	u_long *callers;
	int n_callers;
} function_t;

char *g_trap_100[14] =
{
	"TRAP_MEMSET",
	"TRAP_MEMCPY",
	"TRAP_STRNCPY",
	"TRAP_SIN",
	"TRAP_COS",
	"TRAP_ATAN2",
	"TRAP_SQRT",
	"TRAP_MATRIXMULTIPLY",
	"TRAP_ANGLEVECTORS",
	"TRAP_PERPENDICULARVECTOR",
	"TRAP_FLOOR",
	"TRAP_CEIL",
	"TRAP_TESTPRINTINT",
	"TRAP_TESTPRINTFLOAT"
};

char *qa_syscalls_0[49] =
{
	"G_PRINT",
	"G_ERROR",
	"G_MILLISECONDS",
	"G_CVAR_REGISTER",
	"G_CVAR_UPDATE",
	"G_CVAR_SET",
	"G_CVAR_VARIABLE_INTEGER_VALUE",
	"G_CVAR_VARIABLE_STRING_BUFFER",
	"G_ARGC",
	"G_ARGV",
	"G_FS_FOPEN_FILE",
	"G_FS_READ",
	"G_FS_WRITE",
	"G_FS_FCLOSE_FILE",
	"G_SEND_CONSOLE_COMMAND",
	"G_LOCATE_GAME_DATA",
	"G_DROP_CLIENT",
	"G_SEND_SERVER_COMMAND",
	"G_SET_CONFIGSTRING",
	"G_GET_CONFIGSTRING",
	"G_GET_USERINFO",
	"G_SET_USERINFO",
	"G_GET_SERVERINFO",
	"G_SET_BRUSH_MODEL",
	"G_TRACE",
	"G_POINT_CONTENTS",
	"G_IN_PVS",
	"G_IN_PVS_IGNORE_PORTALS",
	"G_ADJUST_AREA_PORTAL_STATE",
	"G_AREAS_CONNECTED",
	"G_LINKENTITY",
	"G_UNLINKENTITY",
	"G_ENTITIES_IN_BOX",
	"G_ENTITY_CONTACT",
	"G_BOT_ALLOCATE_CLIENT",
	"G_BOT_FREE_CLIENT",
	"G_GET_USERCMD",
	"G_GET_ENTITY_TOKEN",
	"G_FS_GETFILELIST",
	"G_DEBUG_POLYGON_CREATE",
	"G_DEBUG_POLYGON_DELETE",
	"G_REAL_TIME",
	"G_SNAPVECTOR",
	"G_TRACECAPSULE",
	"G_ENTITY_CONTACTCAPSULE",
	"G_FS_SEEK",
    // new since 4.2.001
	"G_NET_STRINGTOADR",
	"G_NET_SENDPACKET",
	"G_SYS_STARTPROCESS",
};

char *qa_syscalls_200[12] =
{
	"BOTLIB_SETUP",
	"BOTLIB_SHUTDOWN",
	"BOTLIB_LIBVAR_SET",
	"BOTLIB_LIBVAR_GET",
	"BOTLIB_PC_ADD_GLOBAL_DEFINE",
	"BOTLIB_START_FRAME",
	"BOTLIB_LOAD_MAP",
	"BOTLIB_UPDATENTITY",
	"BOTLIB_TEST",
	"BOTLIB_GET_SNAPSHOT_ENTITY",
	"BOTLIB_GET_CONSOLE_MESSAGE",
	"BOTLIB_USER_COMMAND"
};

char *qa_syscalls_300[18] =
{
	"BOTLIB_AAS_ENABLE_ROUTING_AREA",
	"BOTLIB_AAS_BBOX_AREAS",
	"BOTLIB_AAS_AREA_INFO",
	"BOTLIB_AAS_ENTITY_INFO",
	"BOTLIB_AAS_INITIALIZED",
	"BOTLIB_AAS_PRESENCE_TYPE_BOUNDING_BOX",
	"BOTLIB_AAS_TIME",
	"BOTLIB_AAS_POINT_AREA_NUM",
	"BOTLIB_AAS_TRACE_AREAS",
	"BOTLIB_AAS_POINT_CONTENTS",
	"BOTLIB_AAS_NEXT_BSP_ENTITY",
	"BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY",
	"BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY",
	"BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY",
	"BOTLIB_AAS_AREA_REACHABILITY",
	"BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA",
	"BOTLIB_AAS_SWIMMING",
	"BOTLIB_AAS_PREDICT_CLIENT_MOVEMENT"
};

char *qa_syscalls_400[24] =
{
	"BOTLIB_EA_SAY",
	"BOTLIB_EA_SAY_TEAM",
	"BOTLIB_EA_COMMAND",
	"BOTLIB_EA_ACTION",
	"BOTLIB_EA_GESTURE",
	"BOTLIB_EA_TALK",
	"BOTLIB_EA_ATTACK",
	"BOTLIB_EA_USE"
	"BOTLIB_EA_RESPAWN",
	"BOTLIB_EA_CROUCH",
	"BOTLIB_EA_MOVE_UP",
	"BOTLIB_EA_MOVE_DOWN",
	"BOTLIB_EA_MOVE_FORWARD",
	"BOTLIB_EA_MOVE_BACK",
	"BOTLIB_EA_MOVE_LEFT",
	"BOTLIB_EA_MOVE_RIGHT",
	"BOTLIB_EA_SELECT_WEAPON",
	"BOTLIB_EA_JUMP",
	"BOTLIB_EA_DELAYED_JUMP",
	"BOTLIB_EA_MOVE",
	"BOTLIB_EA_VIEW",
	"BOTLIB_EA_END_REGULAR",
	"BOTLIB_EA_GET_INPUT",
	"BOTLIB_EA_RESET_INPUT"
};

char *qa_syscalls_500[82] =
{
	"BOTLIB_AI_LOAD_CHARACTER",
	"BOTLIB_AI_FREE_CHARACTER",
	"BOTLIB_AI_CHARACTERISTIC_FLOAT",
	"BOTLIB_AI_CHARACTERISTIC_BFLOAT",
	"BOTLIB_AI_CHARACTERISTIC_INTEGER",
	"BOTLIB_AI_CHARACTERISTIC_BINTEGER",
	"BOTLIB_AI_CHARACTERISTIC_STRING",
	"BOTLIB_AI_ALLOC_CHAT_STATE",
	"BOTLIB_AI_FREE_CHAT_STATE",
	"BOTLIB_AI_QUEUE_CONSOLE_MESSAGE",
	"BOTLIB_AI_REMOVE_CONSOLE_MESSAGE",
	"BOTLIB_AI_NEXT_CONSOLE_MESSAGE",
	"BOTLIB_AI_NUM_CONSOLE_MESSAGE",
	"BOTLIB_AI_INITIAL_CHAT",
	"BOTLIB_AI_REPLY_CHAT",
	"BOTLIB_AI_CHAT_LENGTH",
	"BOTLIB_AI_ENTER_CHAT",
	"BOTLIB_AI_STRING_CONTAINS",
	"BOTLIB_AI_FIND_MATCH",
	"BOTLIB_AI_MATCH_VARIABLE",
	"BOTLIB_AI_UNIFY_WHITE_SPACES",
	"BOTLIB_AI_REPLACE_SYNONYMS",
	"BOTLIB_AI_LOAD_CHAT_FILE",
	"BOTLIB_AI_SET_CHAT_GENDER",
	"BOTLIB_AI_SET_CHAT_NAME",
	"BOTLIB_AI_RESET_GOAL_STATE",
	"BOTLIB_AI_RESET_AVOID_GOALS",
	"BOTLIB_AI_PUSH_GOAL",
	"BOTLIB_AI_POP_GOAL",
	"BOTLIB_AI_EMPTY_GOAL_STACK",
	"BOTLIB_AI_DUMP_AVOID_GOALS",
	"BOTLIB_AI_DUMP_GOAL_STACK",
	"BOTLIB_AI_GOAL_NAME",
	"BOTLIB_AI_GET_TOP_GOAL",
	"BOTLIB_AI_GET_SECOND_GOAL",
	"BOTLIB_AI_CHOOSE_LTG_ITEM",
	"BOTLIB_AI_CHOOSE_NBG_ITEM",
	"BOTLIB_AI_TOUCHING_GOAL",
	"BOTLIB_AI_ITEM_GOAL_IN_VIS_BUT_NOT_VISIBLE",
	"BOTLIB_AI_GET_LEVEL_ITEM_GOAL",
	"BOTLIB_AI_AVOID_GOAL_TIME",
	"BOTLIB_AI_INIT_LEVEL_ITEMS",
	"BOTLIB_AI_UPDATE_ENTITY_ITEMS",
	"BOTLIB_AI_LOAD_ITEM_WEIGHTS",
	"BOTLIB_AI_FREE_ITEM_WEIGHTS",
	"BOTLIB_AI_SAVE_GOAL_FUZZY_LOGIC",
	"BOTLIB_AI_ALLOC_GOAL_STATE",
	"BOTLIB_AI_FREE_GOAL_STATE",
	"BOTLIB_AI_RESET_MOVE_STATE",
	"BOTLIB_AI_MOVE_TO_GOAL",
	"BOTLIB_AI_MOVE_IN_DIRECTION",
	"BOTLIB_AI_RESET_AVOID_REACH",
	"BOTLIB_AI_RESET_LAST_AVOID_REACH",
	"BOTLIB_AI_REACHABILITY_AREA",
	"BOTLIB_AI_MOVEMENT_VIEW_TARGET",
	"BOTLIB_AI_ALLOC_MOVE_STATE",
	"BOTLIB_AI_FREE_MOVE_STATE",
	"BOTLIB_AI_INIT_MOVE_STATE",
	"BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON",
	"BOTLIB_AI_GET_WEAPON_INFO",
	"BOTLIB_AI_LOAD_WEAPON_WEIGHTS",
	"BOTLIB_AI_ALLOC_WEAPON_STATE",
	"BOTLIB_AI_FREE_WEAPON_STATE",
	"BOTLIB_AI_RESET_WEAPON_STATE",
	"BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION",
	"BOTLIB_AI_INTERBREED_GOAL_FUZZY_LOGIC",
	"BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC",
	"BOTLIB_AI_GET_NEXT_CAMP_SPOT_GOAL",
	"BOTLIB_AI_GET_MAP_LOCATION_GOAL",
	"BOTLIB_AI_NUM_INITIAL_CHATS",
	"BOTLIB_AI_GET_CHAT_MESSAGE",
	"BOTLIB_AI_REMOVE_FROM_AVOID_GOALS",
	"BOTLIB_AI_PREDICT_VISIBLE_POSITION",
	"BOTLIB_AI_SET_AVOID_GOAL_TIME",
	"BOTLIB_AI_ADD_AVOID_SPOT",
	"BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL",
	"BOTLIB_AAS_PREDICT_ROUTE",
	"BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX",
	"BOTLIB_PC_LOAD_SOURCE",
	"BOTLIB_PC_FREE_SOURCE",
	"BOTLIB_PC_READ_TOKEN",
	"BOTLIB_PC_SOURCE_FILE_AND_LINE"
};

char *cg_syscalls_0[90] =
{
	"CG_PRINT",
	"CG_ERROR",
	"CG_MILLISECONDS",
	"CG_CVAR_REGISTER",
	"CG_CVAR_UPDATE",
	"CG_CVAR_SET",
	"CG_CVAR_VARIABLESTRINGBUFFER",
	"CG_ARGC",
	"CG_ARGV",
	"CG_ARGS",
	"CG_FS_FOPENFILE",
	"CG_FS_READ",
	"CG_FS_WRITE",
	"CG_FS_FCLOSEFILE",
	"CG_SENDCONSOLECOMMAND",
	"CG_ADDCOMMAND",
	"CG_SENDCLIENTCOMMAND",
	"CG_UPDATESCREEN",
	"CG_CM_LOADMAP",
	"CG_CM_NUMINLINEMODELS",
	"CG_CM_INLINEMODEL",
	"CG_CM_LOADMODEL",
	"CG_CM_TEMPBOXMODEL",
	"CG_CM_POINTCONTENTS",
	"CG_CM_TRANSFORMEDPOINTCONTENTS",
	"CG_CM_BOXTRACE",
	"CG_CM_TRANSFORMEDBOXTRACE",
	"CG_CM_MARKFRAGMENTS",
	"CG_S_STARTSOUND",
	"CG_S_STARTLOCALSOUND",
	"CG_S_CLEARLOOPINGSOUNDS",
	"CG_S_ADDLOOPINGSOUND",
	"CG_S_UPDATEENTITYPOSITION",
	"CG_S_RESPATIALIZE",
	"CG_S_REGISTERSOUND",
	"CG_S_STARTBACKGROUNDTRACK",
	"CG_R_LOADWORLDMAP",
	"CG_R_REGISTERMODEL",
	"CG_R_REGISTERSKIN",
	"CG_R_REGISTERSHADER",
	"CG_R_CLEARSCENE",
	"CG_R_ADDREFENTITYTOSCENE",
	"CG_R_ADDPOLYTOSCENE",
	"CG_R_ADDLIGHTTOSCENE",
	"CG_R_RENDERSCENE",
	"CG_R_SETCOLOR",
	"CG_R_DRAWSTRETCHPIC",
	"CG_R_MODELBOUNDS",
	"CG_R_LERPTAG",
	"CG_GETGLCONFIG",
	"CG_GETGAMESTATE",
	"CG_GETCURRENTSNAPSHOTNUMBER",
	"CG_GETSNAPSHOT",
	"CG_GETSERVERCOMMAND",
	"CG_GETCURRENTCMDNUMBER",
	"CG_GETUSERCMD",
	"CG_SETUSERCMDVALUE",
	"CG_R_REGISTERSHADERNOMIP",
	"CG_MEMORY_REMAINING",
	"CG_R_REGISTERFONT",
	"CG_KEY_ISDOWN",
	"CG_KEY_GETCATCHER",
	"CG_KEY_SETCATCHER",
	"CG_KEY_GETKEY",
 	"CG_PC_ADD_GLOBAL_DEFINE",
	"CG_PC_LOAD_SOURCE",
	"CG_PC_FREE_SOURCE",
	"CG_PC_READ_TOKEN",
	"CG_PC_SOURCE_FILE_AND_LINE",
	"CG_S_STOPBACKGROUNDTRACK",
	"CG_REAL_TIME",
	"CG_SNAPVECTOR",
	"CG_REMOVECOMMAND",
	"CG_R_LIGHTFORPOINT",
	"CG_CIN_PLAYCINEMATIC",
	"CG_CIN_STOPCINEMATIC",
	"CG_CIN_RUNCINEMATIC",
	"CG_CIN_DRAWCINEMATIC",
	"CG_CIN_SETEXTENTS",
	"CG_R_REMAP_SHADER",
	"CG_S_ADDREALLOOPINGSOUND",
	"CG_S_STOPLOOPINGSOUND",
	"CG_CM_TEMPCAPSULEMODEL",
	"CG_CM_CAPSULETRACE",
	"CG_CM_TRANSFORMEDCAPSULETRACE",
	"CG_R_ADDADDITIVELIGHTTOSCENE",
	"CG_GET_ENTITY_TOKEN",
	"CG_R_ADDPOLYSTOSCENE",
	"CG_R_INPVS",
	"CG_FS_SEEK"
};

char *cg_syscalls_100[12] =
{
	"CG_MEMSET",
	"CG_MEMCPY",
	"CG_STRNCPY",
	"CG_SIN",
	"CG_COS",
	"CG_ATAN2",
	"CG_SQRT",
	"CG_FLOOR",
	"CG_CEIL",
	"CG_TESTPRINTINT",
	"CG_TESTPRINTFLOAT",
	"CG_ACOS"
};

char *ui_syscalls_0[93] =
{
	"UI_ERROR",
	"UI_PRINT",
	"UI_MILLISECONDS",
	"UI_CVAR_SET",
	"UI_CVAR_VARIABLEVALUE",
	"UI_CVAR_VARIABLESTRINGBUFFER",
	"UI_CVAR_SETVALUE",
	"UI_CVAR_RESET",
	"UI_CVAR_CREATE",
	"UI_CVAR_INFOSTRINGBUFFER",
	"UI_ARGC",
	"UI_ARGV",
	"UI_CMD_EXECUTETEXT",
	"UI_FS_FOPENFILE",
	"UI_FS_READ",
	"UI_FS_WRITE",
	"UI_FS_FCLOSEFILE",
	"UI_FS_GETFILELIST",
	"UI_R_REGISTERMODEL",
	"UI_R_REGISTERSKIN",
	"UI_R_REGISTERSHADERNOMIP",
	"UI_R_CLEARSCENE",
	"UI_R_ADDREFENTITYTOSCENE",
	"UI_R_ADDPOLYTOSCENE",
	"UI_R_ADDLIGHTTOSCENE",
	"UI_R_RENDERSCENE",
	"UI_R_SETCOLOR",
	"UI_R_DRAWSTRETCHPIC",
	"UI_UPDATESCREEN",
	"UI_CM_LERPTAG",
	"UI_CM_LOADMODEL",
	"UI_S_REGISTERSOUND",
	"UI_S_STARTLOCALSOUND",
	"UI_KEY_KEYNUMTOSTRINGBUF",
	"UI_KEY_GETBINDINGBUF",
	"UI_KEY_SETBINDING",
	"UI_KEY_ISDOWN",
	"UI_KEY_GETOVERSTRIKEMODE",
	"UI_KEY_SETOVERSTRIKEMODE",
	"UI_KEY_CLEARSTATES",
	"UI_KEY_GETCATCHER",
	"UI_KEY_SETCATCHER",
	"UI_GETCLIPBOARDDATA",
	"UI_GETGLCONFIG",
	"UI_GETCLIENTSTATE",
	"UI_GETCONFIGSTRING",
	"UI_LAN_GETPINGQUEUECOUNT",
	"UI_LAN_CLEARPING",
	"UI_LAN_GETPING",
	"UI_LAN_GETPINGINFO",
	"UI_CVAR_REGISTER",
	"UI_CVAR_UPDATE",
	"UI_MEMORY_REMAINING",
	"UI_GET_CDKEY",
	"UI_SET_CDKEY",
	"UI_R_REGISTERFONT",
	"UI_R_MODELBOUNDS",
	"UI_PC_ADD_GLOBAL_DEFINE",
	"UI_PC_LOAD_SOURCE",
	"UI_PC_FREE_SOURCE",
	"UI_PC_READ_TOKEN",
	"UI_PC_SOURCE_FILE_AND_LINE",
	"UI_S_STOPBACKGROUNDTRACK",
	"UI_S_STARTBACKGROUNDTRACK",
	"UI_REAL_TIME",
	"UI_LAN_GETSERVERCOUNT",
	"UI_LAN_GETSERVERADDRESSSTRING",
	"UI_LAN_GETSERVERINFO",
	"UI_LAN_MARKSERVERVISIBLE",
	"UI_LAN_UPDATEVISIBLEPINGS",
	"UI_LAN_RESETPINGS",
	"UI_LAN_LOADCACHEDSERVERS",
	"UI_LAN_SAVECACHEDSERVERS",
	"UI_LAN_ADDSERVER",
	"UI_LAN_REMOVESERVER",
	"UI_CIN_PLAYCINEMATIC",
	"UI_CIN_STOPCINEMATIC",
	"UI_CIN_RUNCINEMATIC",
	"UI_CIN_DRAWCINEMATIC",
	"UI_CIN_SETEXTENTS",
	"UI_R_REMAP_SHADER",
	"UI_VERIFY_CDKEY",
	"UI_LAN_SERVERSTATUS",
	"UI_LAN_GETSERVERPING",
	"UI_LAN_SERVERISVISIBLE",
	"UI_LAN_COMPARESERVERS",
    // new since 4.2.001
	"UI_NET_STRINGTOADR",
	"UI_Q_VSNPRINTF",
	"UI_NET_SENDPACKET",
	"UI_COPYSTRING",
	"UI_SYS_STARTPROCESS",
    // 
	"UI_FS_SEEK",
	"UI_SET_PBCLSTATUS"
};

char *ui_syscalls_100[9] =
{
	"UI_MEMSET",
	"UI_MEMCPY",
	"UI_STRNCPY",
	"UI_SIN",
	"UI_COS",
	"UI_ATAN2",
	"UI_SQRT",
	"UI_FLOOR",
	"UI_CEIL"
};

q3vm_header_t header;

instructionPointer_t *instructionPointers;
int n_instructionPointers;

function_t *functions;
int n_functions;

void exit_usage(char *argv0)
{
	printf("Q3VM Disassembler for Urban Terror by strata\n\n"
	       "\tq - qagame\n"
		   "\tc - cgame\n"
		   "\tu - ui\n\n"
		   "Usage: %s <q,c,u> <file>\n", argv0);

	exit(0);
}

int read_q3vm_header(int);
int load_instructionPointers(int);
int load_functions(void);
void load_calls(void);
void print_disassembly(int, int);
void cleanup(void);

int main(int argc, char **argv)
{
	int fd, fn, i;

	if(argc != 3) {
		exit_usage(argv[0]);
	}

	switch(argv[1][0]) {
		case 'Q':
		case 'q':
			qvmtype = QAGAME;
			break;

		case 'C':
		case 'c':
			qvmtype = CGAME;
			break;

		case 'U':
		case 'u':
			qvmtype = UI;
			break;

		default:
			exit_usage(argv[0]);
	}

	if((fd = open(argv[2], O_RDONLY)) == -1) {
		fprintf(stderr, "Unable to open '%s': %s\n", argv[2],
				strerror(errno));
		return errno;
	}

	if(!read_q3vm_header(fd)) {
		fprintf(stderr, "Invalid QVM header.\n");
		close(fd);
		return 1;
	}

	printf("Q3VM Disassembler for Urban Terror by strata\n\n"
			"Magic...............: 0x%x\n"
			"Instruction count...: %d\n"
			"Code offset.........: 0x%x\n"
			"Code length.........: %d\n"
			"Data offset.........: 0x%x\n"
			"Data length.........: %d\n"
			"LIT length..........: %d\n"
			"BSS length..........: %d\n"
			"Jump targets........: %d\n",
			(u_int) header.magic, (int) header.instructionCount,
			(u_int) header.codeOffset, (int) header.codeLength,
			(u_int) header.dataOffset, (int) header.dataLength,
			(int) header.litLength, (int) header.bssLength,
			(int) header.jtrgLength);

	if((n_instructionPointers = load_instructionPointers(fd)) == 0) {
		fprintf(stderr, "No instructionPointers loaded.\n");
		close(fd);
		return 0;
	}

	if((n_functions = load_functions()) == 0) {
		fprintf(stderr, "No functions found.\n");
		free(instructionPointers);
		close(fd);
		return 0;
	}

	load_calls();

	for(fn = 0; fn < n_functions; fn++) {
		printf("\n\n+++ Function func_0x%x +++\n"
		       "    Frame size.......: %d\n"
			   "    Instruction count: %d, [%d <-> %d]\n"
			   "    Bytecode length..: %d\n",
			   (u_int) functions[fn].startOffset,
			   functions[fn].frameSize,
			   functions[fn].instructionCount,
			   (int) functions[fn].startInstruction,
			   (int) functions[fn].endInstruction,
			   (int) functions[fn].byteSize);

		printf("    Calling..........: %d\n", functions[fn].n_calling);

		if(functions[fn].n_calling) {
			for(i = 0; i < functions[fn].n_calling; i++)
				printf("        func_0x%x\n",
						(u_int) functions[fn].calling[i]);
		}

		printf("    Callers..........: %d\n", functions[fn].n_callers);

		if(functions[fn].n_callers) {
			for(i = 0; i < functions[fn].n_callers; i++)
				printf("        func_0x%x\n", 
						(u_int) functions[fn].callers[i]);
		}

		print_disassembly(fd, fn);
	}


	printf("\nEOF\n");

	cleanup();
	close(fd);

	return 0;
}

int read_q3vm_header(int fd)
{
	read(fd, &header.magic, 4);
	read(fd, &header.instructionCount, 4);
 	read(fd, &header.codeOffset, 4);
	read(fd, &header.codeLength, 4);
	read(fd, &header.dataOffset, 4);
	read(fd, &header.dataLength, 4);
	read(fd, &header.litLength, 4);
	read(fd, &header.bssLength, 4);
	read(fd, &header.jtrgLength, 4);

	if(header.jtrgLength == 0 || header.bssLength == 0 ||
	   header.dataLength == 0 || header.litLength == 0 ||
	   header.codeLength == 0) return 0;

	if(header.instructionCount == 0) return 0;

	return 1;
}

int load_instructionPointers(int fd)
{
	int icount, offset;
	u_char bytecode;

	if((instructionPointers =
				malloc(header.instructionCount *
					sizeof(instructionPointer_t))) == NULL) {
		fprintf(stderr, "Unable to allocate memory: %s\n",
				strerror(errno));
		return 0;
	}

	lseek(fd, header.codeOffset, SEEK_SET);

	icount = offset = 0;

	while(offset < header.codeLength && icount < header.instructionCount) {
		read(fd, &bytecode, 1);

		if(bytecode > NUM_BYTECODES) {
			fprintf(stderr, "Invalid bytecode '%d' at 0x%x\n",
					bytecode, offset);

			offset++;
			continue;
		}

		instructionPointers[icount].offset   = offset;
		instructionPointers[icount].bytecode = bytecode;
		instructionPointers[icount].param    = 0;

		if(bytecodes[bytecode].param_size > 0) {
			instructionPointers[icount].param = 0;
			read(fd, &instructionPointers[icount].param,
					bytecodes[bytecode].param_size);
		}

		offset += (1 + bytecodes[bytecode].param_size);
		icount++;
	}

	printf("Loaded %d instructionPointers\n", icount);

	return icount;
}

int load_functions(void)
{
	int i, j, fn = 0, nf = 0;

	// count them first
	for(i = 0; i < n_instructionPointers; i++)
		if(instructionPointers[i].bytecode == 3) // ENTER
			nf++;

	if((functions = malloc(nf * sizeof(function_t))) == NULL) {
		fprintf(stderr, "Unable to allocate memory: %s\n",
				strerror(errno));
		return 0;
	}

	// load'em up
	for(i = 0; i < n_instructionPointers; i++) {
		if(instructionPointers[i].bytecode != 3) continue;

		// we're inside a function now
		functions[fn].startOffset = instructionPointers[i].offset;
		functions[fn].startInstruction = i;
		functions[fn].frameSize = instructionPointers[i].param;

        functions[fn].n_callers = 0;
        functions[fn].callers = NULL;

		functions[fn].n_calling = 0;
		functions[fn].calling = NULL;
	
		// seek to the end of it.
		for(j = i; j < n_instructionPointers; j++)
			if(instructionPointers[j].bytecode == 4 && // LEAVE
			   instructionPointers[j - 1].bytecode == 6) // PUSH
				 break;

		// +5 for LEAVE $PARM
		functions[fn].endOffset = instructionPointers[j].offset + 5;
		functions[fn].endInstruction = j;

		functions[fn].byteSize =
			functions[fn].endOffset - functions[fn].startOffset;
		functions[fn].instructionCount = 1 + 
			functions[fn].endInstruction - functions[fn].startInstruction;

		fn++;
	}

	printf("Loaded %d functions\n", fn);
	return fn;
}

void find_calling(int fn, int load)
{
	int i, nc = 0, n = 0, dupe;
	int offset;

	for(i = functions[fn].startInstruction;
		i < functions[fn].endInstruction; i++)
	{
		if(instructionPointers[i].bytecode == 8 && // CONST
		   instructionPointers[i + 1].bytecode == 5 && // CALL
		   instructionPointers[i].param < n_instructionPointers)
		{
			if(load == 1) {
				offset = instructionPointers[
							instructionPointers[i].param].offset;

				for(n = 0, dupe = 0; n < nc; n++)
					if(functions[fn].calling[n] == offset) dupe = 1;

				if(dupe == 0)
					functions[fn].calling[nc++] =
						instructionPointers[
							instructionPointers[i].param].offset;
			} else nc++;
		}
	}

	if(load == 0) {
		functions[fn].calling = malloc(nc * sizeof(u_long));
		find_calling(fn, 1);
	} else {
		functions[fn].n_calling = nc;
	}
}

void find_callers(int fn, int load)
{
	int f, c, nc = 0;

	for(f = 0; f < n_functions; f++) {
		if((fn == f) || (functions[f].n_calling == 0))
			continue;

		for(c = 0; c < functions[f].n_calling; c++) {
			if(functions[f].calling[c] == functions[fn].startOffset) {
				if(load == 1)
					functions[fn].callers[nc] = functions[f].startOffset;

				nc++;
			}
		}
	}

	if(load == 0) {
		functions[fn].callers = malloc(nc * sizeof(u_long));
		find_callers(fn, 1);
	} else {
		functions[fn].n_callers = nc;
	}
}

void load_calls(void)
{
	int fn;

	for(fn = 0; fn < n_functions; fn++)
		find_calling(fn, 0);

	for(fn = 0; fn < n_functions; fn++)
		find_callers(fn, 0);

	printf("Loaded all calls\n");
}

void cleanup(void)
{
	int fn;

	free(instructionPointers);

	for(fn = 0; fn < n_functions; fn++) {
		if(functions[fn].n_calling) free(functions[fn].calling);
		if(functions[fn].n_callers) free(functions[fn].callers);
	}

	free(functions);
}

void print_disassembly(int fd, int fn)
{
	char buffer[1024], *syscall;
	int i, si, len;

	printf("\n");

	for(i = functions[fn].startInstruction;
	    i <= functions[fn].endInstruction; i++)
	{
		printf("<0x%08x> (% 6d): %s",
				(u_int) instructionPointers[i].offset, i,
				bytecodes[instructionPointers[i].bytecode].opcode);

		if(bytecodes[instructionPointers[i].bytecode].param_size) {
			printf(" 0x%x", (u_int) instructionPointers[i].param);
		}

		if(instructionPointers[i].bytecode == 8 && // CONST
		   i < functions[fn].endInstruction)
		{
			si = (signed) instructionPointers[i].param;

			// function call
			if(instructionPointers[i + 1].bytecode == 5) { // CALL
				if(instructionPointers[i].param < n_instructionPointers) {
					printf(" [func_0x%x:%d]\n",
							(u_int) instructionPointers[
							instructionPointers[i].param].offset,
							(int) instructionPointers[i].param);
				} else if(si < 0) {
					si = -si;
					si--;

                    // print a syscall
					switch(qvmtype) {
						case QAGAME:
							if(si >= 500 && si <= 582) {
								syscall = qa_syscalls_500[si - 500];
							} else if(si >= 400 && si <= 424) {
								syscall = qa_syscalls_400[si - 400];
							} else if(si >= 300 && si <= 318) {
								syscall = qa_syscalls_300[si - 300];
							} else if(si >= 200 && si <= 212) {
								syscall = qa_syscalls_200[si - 200];
							} else if(si >= 100 && si <= 114) {
								syscall = g_trap_100[si - 100];
							} else if(si >= 0 && si <= 49) {
								syscall = qa_syscalls_0[si];
							} else {
								syscall = "UNKNOWN";
							}

							break;

						case CGAME:
							if(si >= 100 && si <= 112) {
								syscall = cg_syscalls_100[si - 100];
							} else if(si >= 0 && si <= 90) {
								syscall = cg_syscalls_0[si];
							} else {
								syscall = "UNKNOWN";
							}

							break;

						case UI:
							if(si >= 100 && si <= 109) {
								syscall = ui_syscalls_100[si - 100];
							} else if(si >= 0 && si <= 93) {
								syscall = ui_syscalls_0[si];
							} else {
								syscall = "UNKNOWN";
							}

							break;

						default:
							syscall = "UNKNOWN";
					}

					printf(" [syscall_%d:%s]\n", si, syscall);
				} else printf("\n");
			} else {
				if(instructionPointers[i].param >= header.dataLength &&
				   instructionPointers[i].param <= header.dataLength +
				   header.litLength) {
					// literal value (possible string)
					lseek(fd, header.dataOffset +
							instructionPointers[i].param, SEEK_SET);
					len = read(fd, &buffer, sizeof(buffer) - 1);
					buffer[len - 1] = '\0';

					printf(" [\"%s\"]\n", buffer);
				} else printf("\n");
			}
		} else printf("\n");
	}
}
