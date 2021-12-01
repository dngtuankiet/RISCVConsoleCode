#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>

#include <sndfile.h>
#include "fix_fft.h"

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
{	printf ("\nUsage : %s <input file> <output file> <fft file>\n", progname) ;
	puts ("\n"
		"    Where the output file will contain a line for each frame\n"
		"    and a column for each channel.\n"
		) ;

} /* print_usage */

static int
convert_to_text (SNDFILE * infile, FILE * outfile, int channels)
{	int *buf ;
	sf_count_t frames ;
	int k, m, readcount ;
	sf_seek(infile, 0, SEEK_SET  );

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

static int
get_fft (SNDFILE * infile, FILE * outfile, int channels, long int frames)
{	int *buf ;
	short *fft ;
	sf_seek(infile, 0, SEEK_SET  );
	buf = (int *)malloc (frames * channels * sizeof (int)) ;
	if (buf == NULL)
	{	printf ("Error : Out of memory.\n\n") ;
		return 1 ;
		} ;

    // Determinate the number of samples in powers of 2
    int m = 0;
    long int aframes = frames;
    while(aframes > 0) {
        aframes >>= 1;
        m++;
    }

    // Allocate the memory for the fft, which is 2 times N, which is 2**m
    long int siz = 1<<(m+1); // Which is the same as (2**m)*2
    fft = (short *)malloc (siz * sizeof (short)) ;
	if (fft == NULL)
	{	printf ("Error : Out of memory.\n\n") ;
		return 1 ;
		} ;
    memset(fft, 0, siz * sizeof (short));

    // We need to read the exact number of frames (frames is the same as samples)
    long int readframes;
    aframes = BLOCK_SIZE / channels;
    int* bufp = buf;
	int* tbuf = (int *)malloc (BLOCK_SIZE * sizeof (int)) ;
    while((readframes = sf_readf_int (infile, tbuf, aframes)) > 0) {
        memcpy(bufp, tbuf, readframes * sizeof (int));
        bufp += readframes;
    }
	free (tbuf) ;

	// Now, convert the value to short.
	// For 32-bit, Is just dividing by 65535 (or 2^16)
	for (int k = 0 ; k < frames ; k++)
    {
        // The channel is the 1st one
        fft[k] = (short)(buf[k * channels/* + m*/] >> 16);
        } ;

    // Invoke the fft
    fix_fftr(fft, m, 0);

    // Save the FFT
    fprintf (outfile, "# FFT result\n") ;
    fprintf (outfile, "# REAL IMAG\n") ;
    int N = (1 << m);
    for(int k = 0; k < N; k++) {
        fprintf (outfile, "%hd, %hd\n", fft[k*2], fft[k*2+1]) ;
    }

	free (buf) ;
	free (fft) ;

	return 0 ;
} /* convert_to_text */

int
main (int argc, char * argv [])
{	char 		*progname, *infilename, *outfilename, *outfftfilename ;
	SNDFILE		*infile = NULL ;
	FILE		*outfile = NULL ;
	FILE		*outfftfile = NULL ;
	SF_INFO		sfinfo ;
	int 	ret = 1 ;

	progname = strrchr (argv [0], '/') ;
	progname = progname ? progname + 1 : argv [0] ;

	switch (argc)
	{
		case 4 :
			break ;
		default:
			print_usage (progname) ;
			goto cleanup ;
		} ;

	infilename = argv [1] ;
	outfilename = argv [2] ;
	outfftfilename = argv [3] ;

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

	/* Open the output file. */
	if ((outfftfile = fopen (outfftfilename, "w")) == NULL)
	{	printf ("Not able to open output file %s : %s\n", outfftfilename, sf_strerror (NULL)) ;
		goto cleanup ;
		} ;

	fprintf (outfile, "# Converted from file %s.\n", infilename) ;
	fprintf (outfile, "# Channels %d, Sample rate %d, Format %x, Frames %ld\n", sfinfo.channels, sfinfo.samplerate, sfinfo.format, sfinfo.frames) ;

	ret = convert_to_text (infile, outfile, sfinfo.channels) ;
	ret = get_fft (infile, outfftfile, sfinfo.channels, sfinfo.frames) ;

cleanup :

	sf_close (infile) ;
	if (outfile != NULL)
		fclose (outfile) ;

	return ret ;
} /* main */
