#include <stdio.h>

// the number of registers to provide
const int REG_COUNT = 8192;
// this is provides unique values for the names of LLVM SSA variables
static int rv = 1;

// this contains a stack of loop numbers...
static int loopStack[4096];
// and an index to that stack
static int loopStackIndex = 0;

// emit file the header
void emit_header ( FILE* fp )
{
	// first, define @op_out which corresponds to output
	fprintf(fp, "define internal void @op_out(i64 %%val) nounwind\n");
	fprintf(fp, "{\n");
	fprintf(fp, "entry:\n");
	fprintf(fp, "\t%%conv = trunc i64 %%val to i32\n");
	fprintf(fp, "\t%%call = tail call i32 @putchar ( i32 %%conv ) nounwind\n");
	fprintf(fp, "\tret void\n");
	fprintf(fp, "}\n\n");
	fprintf(fp, "declare i32 @putchar(i32) nounwind\n\n");
	// next, define @op_in which corresponds to input
	fprintf(fp, "define internal i64 @op_in() nounwind\n");
	fprintf(fp, "{\n");
	fprintf(fp, "entry:\n");
	fprintf(fp, "\t%%call = tail call i32 @getchar() nounwind\n");
	fprintf(fp, "\t%%conv = sext i32 %%call to i64\n");
	fprintf(fp, "\tret i64 %%conv\n");
	fprintf(fp, "}\n\n");
	fprintf(fp, "declare i32 @getchar() nounwind\n\n");
	// now, define the actual program body
	fprintf(fp, "define void @program() nounwind\n");
	fprintf(fp, "{\n");
	fprintf(fp, "entry:\n");
	// allocate the index stack slot...
	fprintf(fp, "\t%%index = alloca i64, align 8\n");
	// and stack space for the registers
	fprintf(fp, "\t%%registers = alloca [%d x i64], align 8\n", REG_COUNT);
	// set the initial index to 0
	fprintf(fp, "\tstore i64 0, i64* %%index\n");
	// get a pointer to the first element of the registers
	fprintf(fp, "\t%%regroot = getelementptr [%d x i64]* %%registers, i64 0, i64 0\n", REG_COUNT);
	// clear all the registers
	fprintf(fp, "\t%%ptrconv = bitcast i64* %%regroot to i8*\n");
	fprintf(fp, "\tcall void @llvm.memset.i64(i8* %%ptrconv, i8 0, i64 %d, i32 8)\n", REG_COUNT * 8);
}

// emit the footer
void emit_footer ( FILE* fp )
{
	// return a value (void!)
	fprintf(fp, "\tret void\n");
	fprintf(fp, "}\n\n");
	// declaration for the memset intrinsic
	fprintf(fp, "declare void @llvm.memset.i64(i8*, i8, i64, i32)\n");
	// actual C entry point, whcih just calls program
	fprintf(fp, "define i32 @main() nounwind\n");
	fprintf(fp, "{\n");
	fprintf(fp, "entry:\n");
	fprintf(fp, "\tcall void @program() nounwind\n");
	fprintf(fp, "ret i32 0\n");
	fprintf(fp, "}\n\n");
}

// an add is any arithmetic, so a sequence of +s and -s
void emit_add ( FILE* fp, long amount )
{
	int opID = rv++;
	if (amount == 0) // if we're not actually modifying, short-circuit
		return;
	// load the index...
	fprintf(fp, "\t%%idx%d = load i64* %%index\n", opID);
	// load the actual register...
	fprintf(fp, "\t%%ptr%d = getelementptr i64* %%regroot, i64 %%idx%d\n", opID, opID);
	fprintf(fp, "\t%%tmp%d = load i64* %%ptr%d\n", opID, opID);
	// perform the arithmetic...
	fprintf(fp, "\t%%add%d = add i64 %%tmp%d, %ld\n", opID, opID, amount);
	// and store.
	fprintf(fp, "\tstore i64 %%add%d, i64* %%ptr%d\n", opID, opID);
}

// a move moves the register index, so a sequence of >s and <s
void emit_move ( FILE* fp, long amount )
{
	int opID = rv++;
	if (amount == 0)
		return; // short-circuit
	// load the index
	fprintf(fp, "\t%%idx%d = load i64* %%index\n", opID);
	// adjust it
	fprintf(fp, "\t%%add%d = add i64 %%idx%d, %ld\n", opID, opID, amount);
	// store the new index
	fprintf(fp, "\tstore i64 %%add%d, i64* %%index\n", opID);
}

// emits an output operation
void emit_out ( FILE* fp )
{
	int opID = rv++;
	// load the index
	fprintf(fp, "\t%%idx%d = load i64* %%index\n", opID);
	// load the register
	fprintf(fp, "\t%%ptr%d = getelementptr i64* %%regroot, i64 %%idx%d\n", opID, opID);
	fprintf(fp, "\t%%tmp%d = load i64* %%ptr%d\n", opID, opID);
	// pass it to @op_out
	fprintf(fp, "\tcall void @op_out(i64 %%tmp%d) nounwind\n", opID);
}

// emits an input instruction
void emit_in ( FILE* fp )
{
	int opID = rv++;
	// get the input
	fprintf(fp, "\t%%tmp%d = call i64 @op_in() nounwind\n", opID);
	// load the index
	fprintf(fp, "\t%%idx%d = load i64* %%index\n", opID);
	// store the new value
	fprintf(fp, "\t%%ptr%d = getelementptr i64* %%regroot, i64 %%idx%d\n", opID, opID);
	fprintf(fp, "\tstore i64 %%tmp%d, i64* %%ptr%d\n", opID, opID);
}

// this emits a loop header and the beginning of the body, corresponding to a [
void emit_loop_open ( FILE* fp )
{
	// push this entry onto the stack
	int opID = rv++;
	int stackIndex = loopStackIndex++;
	loopStack[stackIndex] = opID;
	// begin the loop header
	fprintf(fp, "\tbr label %%loopHeader%d\n", opID);
	fprintf(fp, "loopHeader%d:\n", opID);
	// the loop header checks the exit condition, so load the index...
	fprintf(fp, "\t%%idx%d = load i64* %%index\n", opID);
	// load the register...
	fprintf(fp, "\t%%ptr%d = getelementptr i64* %%regroot, i64 %%idx%d\n", opID, opID);
	fprintf(fp, "\t%%tmp%d = load i64* %%ptr%d\n", opID, opID);
	// compare to zero...
	fprintf(fp, "\t%%cmp%d = icmp eq i64 %%tmp%d, 0\n", opID, opID);
	// if zero, jump to the corresponding ] which is the end of the loop
	// otherwise, run through the loop body
	fprintf(fp, "\tbr i1 %%cmp%d, label %%loopExit%d, label %%loopBody%d\n", opID, opID, opID);
	// beginning of loop body
	fprintf(fp, "loopBody%d:\n", opID);
}

// this emits the loop exit, which corresponds to ]
void emit_loop_close ( FILE* fp )
{
	// get the top loop from the stack
	int stackIndex = --loopStackIndex;
	int loopID = loopStack[stackIndex];
	// jump to the header, which will jump to the loopExit if the exit condition is met
	fprintf(fp, "\tbr label %%loopHeader%d\n", loopID);
	fprintf(fp, "loopExit%d:\n", loopID);
	// continue
}

// this type contains a reader state
typedef enum
{
	BFP_STATE_ARITHMETIC,
	BFP_STATE_POINTER,
	BFP_STATE_NONE,
} BFP_State;

// this generic function does operation emission, all fairly obvious
static void emit ( FILE* out, BFP_State* state, char lastChar, long amount )
{
	switch (*state)
	{
		case BFP_STATE_ARITHMETIC:
			emit_add(out, amount);
			*state = BFP_STATE_NONE;
			break;
		case BFP_STATE_POINTER:
			emit_move(out, amount);
			*state = BFP_STATE_NONE;
			break;
		case BFP_STATE_NONE:
			switch (lastChar)
			{
				case '.':
					emit_out(out);
					break;
				case ',':
					emit_in(out);
					break;
				case '[':
					emit_loop_open(out);
					break;
				case ']':
					emit_loop_close(out);
					break;
			}
			break;
	}
}

// the real meat of the compiler - read in as brainfuck, compile to out as LLVM IR
void bfp ( FILE* in, FILE* out )
{
	long amount = 0;
	BFP_State state = BFP_STATE_NONE;
	// emit the header
	emit_header(out);
	// loop through everything
	while (!feof(in))
	{
		// basically, a finite state machine
		char ch;
		fread(&ch, 1, 1, in);
		if (state == BFP_STATE_ARITHMETIC) // we're doing arithmetic at the moment! go us
		{
			// if it's a + or -, adjust the working value
			if (ch == '+')
				amount++;
			else if (ch == '-')
				amount--;
			else
			{
				// it's something else! emit the instruction, push it back to be read next loop
				emit(out, &state, ch, amount);
				ungetc(ch, in);
				amount = 0;
			}
		}
		else if (state == BFP_STATE_POINTER) // same as above but for pointers
		{
			if (ch == '>')
				amount++;
			else if (ch == '<')
				amount--;
			else
			{
				emit(out, &state, ch, amount);
				ungetc(ch, in);
				amount = 0;
			}
		}
		else // just emit an operation or enter the appropriate state
		{
			if (ch == '+')
			{
				state = BFP_STATE_ARITHMETIC;
				amount = 1;
			}
			else if (ch == '-')
			{
				state = BFP_STATE_ARITHMETIC;
				amount = -1;
			}
			else if (ch == '>')
			{
				state = BFP_STATE_POINTER;
				amount = 1;
			}
			else if (ch == '<')
			{
				state = BFP_STATE_POINTER;
				amount = -1;
			}
			else
			{
				emit(out, &state, ch, 0);
			}
		}
	}
	// emit any trailing +s or -s (maybe this should be removed, as it won't contribute to the output?)
	emit(out, &state, 0, amount);
	emit_footer(out);
}

int main ()
{
	// open up llvm-as | opt -std-compile-opts | lli as a pipe
	// so that we pipe everything to the LLVM toolchain
	FILE* asOut;
	asOut = popen("llvm-as | opt -std-compile-opts | lli", "w");
	// run on stdin
	bfp(stdin, asOut);
	// tidy up
	pclose(asOut);
	return 0;
}
