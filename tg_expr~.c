/**
	@file
	tg_expr~: a simple audio object for Max
	original by: jeremy bernstein, jeremy@bootsquad.com	
	@ingroup examples	
*/

#include "ext.h"			// standard Max include, always required (except in Jitter)
#include "ext_obex.h"		// required for "new" style objects
#include "z_dsp.h"			// required for MSP objects

#include "expression_parser.h"


// struct to represent the object's state
typedef struct _tg_expr {
	t_pxobject		ob;			// the object itself (t_pxobject in MSP instead of t_object)
    char expr[256];
    double **in_sigs;
    int sample;
} t_tg_expr;


// method prototypes
void *tg_expr_new(t_symbol *s, long argc, t_atom *argv);
void tg_expr_free(t_tg_expr *x);
void tg_expr_assist(t_tg_expr *x, void *b, long m, long a, char *s);
void tg_expr_float(t_tg_expr *x, double f);
void tg_expr_dsp64(t_tg_expr *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void tg_expr_perform64(t_tg_expr *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);

void tg_expr_set(t_tg_expr *x, t_symbol *s, long argc, t_atom *argv);


// global class pointer variable
static t_class *tg_expr_class = NULL;

int tg_expr_var_callback(void *user_data, const char *name, double *value);


//***********************************************************************************************

int C74_EXPORT main(void)
{	
	// object initialization, note the use of dsp_free for the freemethod, which is required
	// unless you need to free allocated memory, in which case you should call dsp_free from
	// your custom free function.

	t_class *c = class_new("tg.expr~", (method)tg_expr_new, (method)dsp_free, (long)sizeof(t_tg_expr), 0L, A_GIMME, 0);
	
	class_addmethod(c, (method)tg_expr_dsp64, "dsp64", A_CANT, 0);		// New 64-bit MSP dsp chain compilation for Max 6
	class_addmethod(c, (method)tg_expr_assist, "assist", A_CANT, 0);
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
		dsp_setup((t_pxobject *)x, 4);	// MSP inlets: arg is # of inlets and is REQUIRED!
										// use 0 if you don't need inlets
		outlet_new(x, "signal"); 		// signal outlet (note "signal" rather than NULL)
        strcpy(x->expr, "v0");
	}
	return (x);
}


// NOT CALLED!, we use dsp_free for a generic free function
void tg_expr_free(t_tg_expr *x) 
{
	;
}


//***********************************************************************************************

void tg_expr_assist(t_tg_expr *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) { //inlet
		sprintf(s, "I am inlet %ld", a);
	} 
	else {	// outlet
		sprintf(s, "I am outlet %ld", a); 			
	}
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

//***********************************************************************************************

// this is the Max 6 version of the dsp method -- it registers a function for the signal chain in Max 6,
// which operates on 64-bit audio signals.
void tg_expr_dsp64(t_tg_expr *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	post("my sample rate is: %f", samplerate);
	
	// instead of calling dsp_add(), we send the "dsp_add64" message to the object representing the dsp chain
	// the arguments passed are:
	// 1: the dsp64 object passed-in by the calling function
	// 2: the symbol of the "dsp_add64" message we are sending
	// 3: a pointer to your object
	// 4: a pointer to your 64-bit perform method
	// 5: flags to alter how the signal chain handles your object -- just pass 0
	// 6: a generic pointer that you can use to pass any additional data to your perform method
	
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
    
    // increment ap each time to get to the next atom
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
    post("expression was: %s", x->expr);
    strcpy(x->expr, expr);
    post("expression is now: %s", x->expr);
}


//***********************************************************************************************

// this is 64-bit perform method for Max 6
void tg_expr_perform64(t_tg_expr *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	t_double *outL = outs[0];	// we get audio for each outlet of the object from the **outs argument

    x->in_sigs = ins;
    for (x->sample = 0; x->sample < sampleframes; x->sample++) {
        *outL++ = our_parse_expression_with_callbacks(x->expr, tg_expr_var_callback, NULL, x);
    }
}

