#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>

#include <sndfile.h>

#define	BLOCK_SIZE 4096

#ifdef DBL_DECIMAL_DIG
	#define OP_DBL_Digs (DBL_DECIMAL_DIG)
#else
	#ifdef DECIMAL_DIG
		#define OP_DBL_Digs (DECIMAL_DIG)
	#else
		#define OP_DBL_Digs (DBL_DIG + 3)
	#endif
#endif

static void
print_usage (char *progname)
{	printf ("\nUsage : %s [--full-precision] <input file> <output file>\n", progname) ;
	puts ("\n"
		"    Where the output file will contain a line for each frame\n"
		"    and a column for each channel.\n"
		) ;

} /* print_usage */

static int
convert_to_text (SNDFILE * infile, FILE * outfile, int channels, int full_precision)
{	int *buf ;
	sf_count_t frames ;
	int k, m, readcount ;

	buf = (int *)malloc (BLOCK_SIZE * sizeof (int)) ;
	if (buf == NULL)
	{	printf ("Error : Out of memory.\n\n") ;
		return 1 ;
		} ;

	frames = BLOCK_SIZE / channels ;

	while ((readcount = (int) sf_readf_int (infile, buf, frames)) > 0)
	{	for (k = 0 ; k < readcount ; k++)
		{	for (m = 0 ; m < channels ; m++)
				fprintf (outfile, " %d", buf [k * channels + m]);
			fprintf (outfile, "\n") ;
			} ;
		} ;

	free (buf) ;

	return 0 ;
} /* convert_to_text */

int
main (int argc, char * argv [])
{	char 		*progname, *infilename, *outfilename ;
	SNDFILE		*infile = NULL ;
	FILE		*outfile = NULL ;
	SF_INFO		sfinfo ;
	int		full_precision = 0 ;
	int 	ret = 1 ;

	progname = strrchr (argv [0], '/') ;
	progname = progname ? progname + 1 : argv [0] ;

	switch (argc)
	{	case 4 :
			if (!strcmp ("--full-precision", argv [3]))
			{	print_usage (progname) ;
				goto cleanup ;
				} ;
			full_precision = 1 ;
			argv++ ;
		case 3 :
			break ;
		default:
			print_usage (progname) ;
			goto cleanup ;
		} ;

	infilename = argv [1] ;
	outfilename = argv [2] ;

	if (strcmp (infilename, outfilename) == 0)
	{	printf ("Error : Input and output filenames are the same.\n\n") ;
		print_usage (progname) ;
		goto cleanup ;
		} ;

	if (infilename [0] == '-')
	{	printf ("Error : Input filename (%s) looks like an option.\n\n", infilename) ;
		print_usage (progname) ;
		goto cleanup ;
		} ;

	if (outfilename [0] == '-')
	{	printf ("Error : Output filename (%s) looks like an option.\n\n", outfilename) ;
		print_usage (progname) ;
		goto cleanup ;
		} ;

	memset (&sfinfo, 0, sizeof (sfinfo)) ;

	if ((infile = sf_open (infilename, SFM_READ, &sfinfo)) == NULL)
	{	printf ("Not able to open input file %s.\n", infilename) ;
		puts (sf_strerror (NULL)) ;
		goto cleanup ;
		} ;

	/* Open the output file. */
	if ((outfile = fopen (outfilename, "w")) == NULL)
	{	printf ("Not able to open output file %s : %s\n", outfilename, sf_strerror (NULL)) ;
		goto cleanup ;
		} ;

	fprintf (outfile, "# Converted from file %s.\n", infilename) ;
	fprintf (outfile, "# Channels %d, Sample rate %d, Format %x\n", sfinfo.channels, sfinfo.samplerate, sfinfo.format) ;

	ret = convert_to_text (infile, outfile, sfinfo.channels, full_precision) ;

cleanup :

	sf_close (infile) ;
	if (outfile != NULL)
		fclose (outfile) ;

	return ret ;
} /* main */
