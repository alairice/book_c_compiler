#include "ucl.h"
#include "ast.h"
#include "decl.h"
#include "expr.h"
#include "gen.h"
#include "output.h"
#include "target.h"


#include "config.h"

int SwitchTableNum;

/**
 * Emit all the strings to assembly file
 */
static void EmitStrings(void)
{
	Symbol p = Strings;
	String str;
	int len;
	int size;

	while (p)
	{
		DefineGlobal(p);
		str = p->val.p;
		/* assert(p->ty->categ == ARRAY); */
		len = strlen(p->aname);
		size = str->len + 1;
		if (p->ty->bty == WCharType){
			int i = 0;
			union value val;
			UCC_WC_T *wcp = (UCC_WC_T *)str->chs;

			val.i[1] = 0;
			while (i < size)
			{
				val.i[0] = wcp[i];
				DefineValue(WCharType, val);
				LeftAlign(ASMFile, len);
				PutString("\t");
				i++;
			}
			PutString("\n");
		}
		else
		{
			DefineString(str, size);
		}
		p = p->next;
	}
	PutString("\n");
}

void EmitFloatConstants(void)
{
	Symbol p = FloatConstants;

	while (p)
	{
		DefineFloatConstant(p);
		p = p->next;
	}
	PutString("\n");
}

static void EmitGlobals(void)
{
	Symbol p = Globals;
	InitData initd;
	int len, size;


	int changed = 0;

	while (p)
	{
		initd = AsVar(p)->idata;
		/**
		added for empty struct or array of empty struct,
			we can change the size to 1 here .
		 */
		changed = 0; 
		if(p->ty->size == 0){
			changed = 1;
			/* PRINT_DEBUG_INFO(("%s",p->name)); */
			p->ty->size = EMPTY_OBJECT_SIZE;
			p->ty->align = EMPTY_OBJECT_ALIGN;
		}

		if (p->sclass == TK_EXTERN && initd == NULL)
		{
			if (p->ref > 0)
			{
				Import(p);
			}
		}
		else if (initd == NULL)
		{
			DefineCommData(p);
		}
		else 
		{
			DefineGlobal(p);
			len = strlen(p->aname);
			size = 0;
			while (initd)
			{
				/* PRINT_DEBUG_INFO(("offset = %d ,size =  %d, val = %d",initd->offset,size, initd->expr->val.i[0])); */
				if (initd->offset != size)
				{
					LeftAlign(ASMFile, len);
					PutString("\t");					
					Space(initd->offset - size);
				}
				if (initd->offset != 0)
				{
					LeftAlign(ASMFile, len);
					PutString("\t");
				}
				if (initd->expr->op == OP_ADD)
				{
					int n = initd->expr->kids[1]->val.i[0];
	
					DefineAddress(initd->expr->kids[0]->val.p);
					if (n != 0)
					{
						Print("%s%d", n > 0 ? " + " : " ", n);
					}
					PutString("\n");
				}
				else if (initd->expr->op == OP_STR)
				{
					String str = initd->expr->val.p;
					size = initd->expr->ty->size / initd->expr->ty->bty->size;
					if (initd->expr->ty->bty == WCharType)	{
						int i = 0;
						union value val;
						UCC_WC_T * wcp = (UCC_WC_T *) str->chs;

						val.i[1] = 0;
						while (i < size)
						{
							val.i[0] = wcp[i];
							DefineValue(WCharType, val);
							LeftAlign(ASMFile, len);
							PutString("\t");
							i++;
						}
					}
					else
					{
						DefineString(str, size);
					}
				}
				else
				{
					DefineValue(initd->expr->ty, initd->expr->val);
				}
				size = initd->offset + initd->expr->ty->size;
				initd = initd->next;
			}
			if (size < p->ty->size)
			{
				LeftAlign(ASMFile, len);
				PutString("\t");
				Space(p->ty->size - size);
			}
			PutString("\n");
		}
		/* restore the zero size , though it is not needed here. */
		if(changed && p->ty->size == EMPTY_OBJECT_SIZE){
			p->ty->size = 0;
			changed = 0;
		}
		p = p->next;
	}
	PutString("\n");
}
/**
	Output some directives in assembly file to declare external funcions:
		Print("EXTRN %s:NEAR32\n\n", GetAccessName(p));
 */
static void ImportFunctions(void)
{
	Symbol p = Functions;

	while (p)
	{
		/*
			If it is a function undefined and referenced in current tranlate unit,
			declare the extern functions in  asm file.
		 */
		if (! p->defined && p->ref)
		{
			Import(p);
		}
		p = p->next;
	}
}

/**
 * Emit all the functions
 */
static void EmitFunctions(AstTranslationUnit transUnit)
{
	AstNode p;
	FunctionSymbol fsym;

	p = transUnit->extDecls;
	while (p != NULL)
	{
		if (p->kind == NK_Function)
		{
			fsym = ((AstFunction)p)->fsym;
			if (fsym->sclass != TK_STATIC || fsym->ref > 0)
			{
				EmitFunction(fsym);
			}
		}
		p = p->next;
	}
}

/**
 * Emit  assembly code for the translation unit
 */
void EmitTranslationUnit(AstTranslationUnit transUnit)
{
	if(ASMFileName){
		ASMFile = fopen(ASMFileName, "w");
		ASMFileName = NULL;
	}else{
		ASMFile = CreateOutput(Input.filename, ExtName);
	}
	SwitchTableNum = 1;
	/* "# Code auto-generated by UCC\n\n" */
	BeginProgram();
	/* ".data\n\n" */
	Segment(DATA);
	/**
		.str0:	.string	"%d \012"
		.str1:	.string	"a + b + c + d = %d.\012"
	 */
	EmitStrings();

	EmitFloatConstants();

	EmitGlobals();
	/* ".text\n\n" */
	Segment(CODE);

	ImportFunctions();
	/**
		The key function is 
			void EmitFunction(FunctionSymbol fsym)
		in x86.c
	 */
	EmitFunctions(transUnit);

	EndProgram();

	fclose(ASMFile);
}
