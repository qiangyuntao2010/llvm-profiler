#ifndef CORELAB_CAMP_COMMON_H
#define CORELAB_CAMP_COMMON_H

#include <inttypes.h>
#include <stdio.h>

#ifdef __GNUC__
#include <tr1/unordered_set>
using namespace std::tr1;
#else
#include <unordered_set>
#endif

// Additional Syntax Modifier
#define OUT 											// (Function argument modifier) argument which 'dedicated for RETURN'
#define UNUSED 										// (Formal modifier) specifies unused entity

#define MAX_CNTX_LV 9 						// Maximum Context Level
#define MIN_CNTX_LV 0 						// Minimum Context Level. XXX JUST FOR REFERENCE. NOT USED

// XXX MUST UPDATE FOLLOWINGS IN CASE OF MAX_CNTX_LV CHANGED XXX
#define STK_CNTXID_SIZE 	20 			// sizeof(CntxID) * (MAX_CNTX_LV + 1)
#define STK_ITERCNT_SIZE 	10 			// sizeof(IterCnt) * (MAX_CNTX_LV + 1)
#define CONTEXT_ITER_CNT_MERGED 128 	// 0b 1000 0000

using namespace std;

namespace corelab
{
	namespace CAMP
	{
		/* (Typedef) CntxID/InstrID
				ID type of Context (Call/Loop) & Instruction (Load/Store)
				XXX InstrID: ONLY LEAST-SIGNIFICANT 15-BITS ARE VALID XXX*/
		typedef uint16_t 	CntxID;
		typedef uint16_t 	InstrID;
		
		/* (Typedef) IterCnt
				Type of Iteration counter */
		typedef uint8_t 	IterCnt;
		/* In binary, MSB is Mix bit telling that multiple iteration counter value are mixed in one iter counter */
		//refer CONTEXT_ITER_CNT_MERGED

		/* (Typedef) FunctionID/BBID 
				ID type of Function & Basic Block */
		typedef uint16_t 	FuncID;
		typedef uint16_t 	BBID;

		/* (Typedef) DepElemID
				ID type of DEP_ELEM struct

				------------------------------------
				|        SIID        |     SEQ     |
				------------------------------------
				24                   9             0

				BITS 				SIZE 	NAME 		USAGE
				-------------------------------------------------------
				[0 - 8]			9 		SEQ 		Sequencial index
				[9 - 23] 		15 		SIID 	 	Source Instruction ID

				XXX ONLY LEAST-SIGNIFICANT 24-BITS ARE VALID XXX */
		#define DEP_ELEM_SIID_MASK 	0xFFFFFE00
		#define DEP_ELEM_SEQ_MASK 	0x000001FF
		#define DEP_ELEM_SIID_OFF 	9
		#define DEP_ELEM_SEQ_OFF 		0
		typedef uint32_t 	DepElemID;
		
		typedef unordered_set<DepElemID>	DepElemIDSet;

		/* (Struct) DepElem
				Dependence Table Element */
		typedef struct
		{
			InstrID 	srcInstrID;												// Source Instruction ID
			InstrID 	dstInstrID;												// Destination Instrction ID
			CntxID 		srcStkCntxID[MAX_CNTX_LV + 1];		// Source Context ID Stack
			CntxID 		dstStkCntxID[MAX_CNTX_LV + 1];		// Destination Context ID Stack

			uint32_t 	loopDep;						// Accumulative Loop Dependence Bit Vector
		} DepElem;

		// /* (Enum) LoopDepType
		// 		Specifies 2bit-unit Loop Dependence Type Representation */
		// typedef enum
		// {
		// 	NONE 	= 0x00,
		// 	INTRA = 0x01,
		// 	INTER = 0x02,
		// 	MIXED = 0x03 
		// } LoopDepElem;

		/* (Enum) InstrType
				Specifies Intruction Type
				XXX ENUM-VALUE SENSITIVE! DO NOT CHANGE THIS!! XXX */
		typedef enum
		{
			LOAD 	= 0x00,
			STORE = 0x01
		} InstrType;
	}
}

#endif	
