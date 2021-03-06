/**
	@file
	tg_expr~: Puredata like expr~ for max/msp
	@author tom.gascoigne
*/

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include "expression_parser.h"

typedef struct _tg_expr {
	t_pxobject ob;
	char expr[256];
	double **in_sigs;
	int sample;
} t_tg_expr;

void *tg_expr_new(t_symbol *s, long argc, t_atom *argv);
void tg_expr_free(t_tg_expr *x);
void tg_expr_dsp64(t_tg_expr *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void tg_expr_perform64(t_tg_expr *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void tg_expr_set(t_tg_expr *x, t_symbol *s, long argc, t_atom *argv);
int tg_expr_var_callback(void *user_data, const char *name, double *value);

static t_class *tg_expr_class = NULL;

int C74_EXPORT main(void)
{
	t_class *c = class_new("tg.expr~", (method)tg_expr_new, (method)dsp_free, (long)sizeof(t_tg_expr), 0L, A_GIMME, 0);
	
	class_addmethod(c, (method)tg_expr_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(c, (method)tg_expr_set, "set", A_GIMME, 0);
	
	class_dspinit(c);
	class_register(CLASS_BOX, c);
	tg_expr_class = c;

	return 0;
}

void *tg_expr_new(t_symbol *s, long argc, t_atom *argv)
{
	t_tg_expr *x = (t_tg_expr *)object_alloc(tg_expr_class);

	if (x) {
		dsp_setup((t_pxobject *)x, 4);
		outlet_new(x, "signal");
		strcpy(x->expr, "v0");
	}
	return (x);
}

// NOT CALLED!, we use dsp_free for a generic free function
void tg_expr_free(t_tg_expr *x) 
{
	
}

int tg_expr_var_callback(void *user_data, const char *name, double *value)
{
    t_tg_expr* x = (t_tg_expr*) user_data;
    if (name[0] == 'v') {
        int sig_num = name[1] - 48;
        *value = x->in_sigs[sig_num][x->sample];
        return PARSER_TRUE;
    }
    return PARSER_FALSE;
}

void tg_expr_dsp64(t_tg_expr *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	post("my sample rate is: %f", samplerate);
	
	object_method(dsp64, gensym("dsp_add64"), x, tg_expr_perform64, 0, NULL);
}

double our_parse_expression_with_callbacks( const char *expr, parser_variable_callback variable_cb, parser_function_callback function_cb, void *user_data ){
	double val;
	parser_data pd;
	parser_data_init( &pd, expr, variable_cb, function_cb, user_data );
	val = parser_parse( &pd );
	if( pd.error ){
		post("Error: %s\n", pd.error );
		post("Expression '%s' failed to parse, returning nan\n", expr );
	}
	return val;	
}

void tg_expr_set(t_tg_expr *x, t_symbol *s, long argc, t_atom *argv)
{
	long i;
	t_atom *ap;
	char expr[256];
	char buf[32];
	strcpy(expr, "");
    
	for (i = 0, ap = argv; i < argc; i++, ap++) {
		switch (atom_gettype(ap)) {
			case A_LONG:
				sprintf(buf, "%lld", atom_getlong(ap));
				break;
			case A_FLOAT:
				sprintf(buf, "%.2f", atom_getfloat(ap));
				break;
			case A_SYM:
				sprintf(buf, "%s", atom_getsym(ap)->s_name);
				break;
			default:
				post("ERR %ld: unknown atom type (%ld)", i+1, atom_gettype(ap));
				break;
		}
		strcat (expr, " ");
		strcat (expr, buf);
	}
	strcpy(x->expr, expr);
}

void tg_expr_perform64(t_tg_expr *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	t_double *outL = outs[0];

	x->in_sigs = ins;
	for (x->sample = 0; x->sample < sampleframes; x->sample++) {
		*outL++ = our_parse_expression_with_callbacks(x->expr, tg_expr_var_callback, NULL, x);
	}
}

