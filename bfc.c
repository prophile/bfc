#include <stdio.h>

const int REG_COUNT = 8192;
static int rv = 1;

static int loopStack[4096];
static int loopStackIndex = 0;

void emit_header ( FILE* fp )
{
	fprintf(fp, "define internal void @op_out(i64 %%val) nounwind\n");
	fprintf(fp, "{\n");
	fprintf(fp, "entry:\n");
	fprintf(fp, "\t%%conv = trunc i64 %%val to i32\n");
	fprintf(fp, "\t%%call = tail call i32 @putchar ( i32 %%conv ) nounwind\n");
	fprintf(fp, "\tret void\n");
	fprintf(fp, "}\n\n");
	fprintf(fp, "declare i32 @putchar(i32) nounwind\n\n");
	fprintf(fp, "define internal i64 @op_in() nounwind\n");
	fprintf(fp, "{\n");
	fprintf(fp, "entry:\n");
	fprintf(fp, "\t%%call = tail call i32 @getchar() nounwind\n");
	fprintf(fp, "\t%%conv = sext i32 %%call to i64\n");
	fprintf(fp, "\tret i64 %%conv\n");
	fprintf(fp, "}\n\n");
	fprintf(fp, "declare i32 @getchar() nounwind\n\n");
	fprintf(fp, "define void @program() nounwind\n");
	fprintf(fp, "{\n");
	fprintf(fp, "entry:\n");
	fprintf(fp, "\t%%index = alloca i64, align 8\n");
	fprintf(fp, "\t%%registers = alloca [%d x i64], align 8\n", REG_COUNT);
	fprintf(fp, "\tstore i64 0, i64* %%index\n");
	fprintf(fp, "\t%%regroot = getelementptr [%d x i64]* %%registers, i64 0, i64 0\n", REG_COUNT);
	fprintf(fp, "\t%%ptrconv = bitcast i64* %%regroot to i8*\n");
	fprintf(fp, "\tcall void @llvm.memset.i64(i8* %%ptrconv, i8 0, i64 %d, i32 8)\n", REG_COUNT * 8);
}

void emit_footer ( FILE* fp )
{
	fprintf(fp, "\tret void\n");
	fprintf(fp, "}\n\n");
	fprintf(fp, "declare void @llvm.memset.i64(i8*, i8, i64, i32)\n");
	fprintf(fp, "define i32 @main() nounwind\n");
	fprintf(fp, "{\n");
	fprintf(fp, "entry:\n");
	fprintf(fp, "\tcall void @program() nounwind\n");
	fprintf(fp, "ret i32 0\n");
	fprintf(fp, "}\n\n");
}

void emit_add ( FILE* fp, long amount )
{
	int opID = rv++;
	fprintf(fp, "\t%%idx%d = load i64* %%index\n", opID);
	fprintf(fp, "\t%%ptr%d = getelementptr i64* %%regroot, i64 %%idx%d\n", opID, opID);
	fprintf(fp, "\t%%tmp%d = load i64* %%ptr%d\n", opID, opID);
	fprintf(fp, "\t%%add%d = add i64 %%tmp%d, %ld\n", opID, opID, amount);
	fprintf(fp, "\tstore i64 %%add%d, i64* %%ptr%d\n", opID, opID);
}

void emit_move ( FILE* fp, long amount )
{
	int opID = rv++;
	fprintf(fp, "\t%%idx%d = load i64* %%index\n", opID);
	fprintf(fp, "\t%%add%d = add i64 %%idx%d, %ld\n", opID, opID, amount);
	fprintf(fp, "\tstore i64 %%add%d, i64* %%index\n", opID);
}

void emit_out ( FILE* fp )
{
	int opID = rv++;
	fprintf(fp, "\t%%idx%d = load i64* %%index\n", opID);
	fprintf(fp, "\t%%ptr%d = getelementptr i64* %%regroot, i64 %%idx%d\n", opID, opID);
	fprintf(fp, "\t%%tmp%d = load i64* %%ptr%d\n", opID, opID);
	fprintf(fp, "\tcall void @op_out(i64 %%tmp%d) nounwind\n", opID);
}

void emit_in ( FILE* fp )
{
	int opID = rv++;
	fprintf(fp, "\t%%tmp%d = call i64 @op_in() nounwind\n", opID);
	fprintf(fp, "\t%%idx%d = load i64* %%index\n", opID);
	fprintf(fp, "\t%%ptr%d = getelementptr i64* %%regroot, i64 %%idx%d\n", opID, opID);
	fprintf(fp, "\tstore i64 %%tmp%d, i64* %%ptr%d\n", opID, opID);
}

void emit_loop_open ( FILE* fp )
{
	int opID = rv++;
	int stackIndex = loopStackIndex++;
	loopStack[stackIndex] = opID;
	fprintf(fp, "\tbr label %%loopHeader%d\n", opID);
	fprintf(fp, "loopHeader%d:\n", opID);
	fprintf(fp, "\t%%idx%d = load i64* %%index\n", opID);
	fprintf(fp, "\t%%ptr%d = getelementptr i64* %%regroot, i64 %%idx%d\n", opID, opID);
	fprintf(fp, "\t%%tmp%d = load i64* %%ptr%d\n", opID, opID);
	fprintf(fp, "\t%%cmp%d = icmp eq i64 %%tmp%d, 0\n", opID, opID);
	fprintf(fp, "\tbr i1 %%cmp%d, label %%loopExit%d, label %%loopBody%d\n", opID, opID, opID);
	fprintf(fp, "loopBody%d:\n", opID);
}

void emit_loop_close ( FILE* fp )
{
	int stackIndex = --loopStackIndex;
	int loopID = loopStack[stackIndex];
	fprintf(fp, "\tbr label %%loopHeader%d\n", loopID);
	fprintf(fp, "loopExit%d:\n", loopID);
}

typedef enum
{
	BFP_STATE_ARITHMETIC,
	BFP_STATE_POINTER,
	BFP_STATE_NONE,
} BFP_State;

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

void bfp ( FILE* in, FILE* out )
{
	long amount = 0;
	BFP_State state = BFP_STATE_NONE;
	emit_header(out);
	while (!feof(in))
	{
		char ch;
		fread(&ch, 1, 1, in);
		if (state == BFP_STATE_ARITHMETIC)
		{
			if (ch == '+')
				amount++;
			else if (ch == '-')
				amount--;
			else
			{
				emit(out, &state, ch, amount);
				ungetc(ch, in);
				amount = 0;
			}
		}
		else if (state == BFP_STATE_POINTER)
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
		else
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
	emit(out, &state, 0, amount);
	emit_footer(out);
}

int main ()
{
	FILE* asOut;
	asOut = popen("llvm-as | opt -std-compile-opts | lli", "w");
	bfp(stdin, asOut);
	pclose(asOut);
	return 0;
}
