/*
 * cont_adseg.c -- Continuously listen and segment input speech into utterances.
 * 
 * HISTORY
 * 
 * 27-Jun-96	M K Ravishankar (rkm@cs.cmu.edu) at Carnegie Mellon University
 * 		Created.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "s2types.h"
#include "ad.h"
#include "cont_ad.h"
#include "err.h"

/*
 * Segment raw A/D input data into utterances whenever silence region of given
 * duration is encountered.
 * Utterances are written to files named 0001.raw, 0002.raw, 0003.raw, etc.
 */
int
main (int32 argc, char **argv)
{
    ad_rec_t *ad;
    cont_ad_t *cont;
    int32 k, uttno, ts, uttlen, sps, endsilsamples;
    float endsil;
    int16 buf[4096];
    FILE *fp;
    char file[1024];
    
    if ((argc != 3) ||
	(sscanf (argv[1], "%d", &sps) != 1) ||
	(sscanf (argv[2], "%f", &endsil) != 1) || (endsil <= 0.0)) {
	E_FATAL("Usage: %s <sampling-rate> <utt-end-sil(sec)>\n", argv[0]);
    }

    /* Convert desired min. inter-utterance silence duration to #samples */
    endsilsamples = (int32) (endsil * sps);

    /* Open raw A/D device */
    if ((ad = ad_open_sps (sps)) == NULL)
	E_FATAL("ad_open_sps(%d) failed\n", sps);
    
    /* Associate new continuous listening module with opened raw A/D device */
    if ((cont = cont_ad_init (ad, ad_read)) == NULL)
	E_FATAL("cont_ad_init failed\n");
    
    /* Calibrate continuous listening for background noise/silence level */
    printf ("Calibrating ..."); fflush (stdout);
    ad_start_rec (ad);
    if (cont_ad_calib (cont) < 0)
	printf (" failed\n");
    else
	printf (" done\n");
    
    /* Forever listen for utterances */
    printf ("You may speak now\n"); fflush (stdout);
    uttno = 0;
    for (;;) {
	/* Wait for beginning of next utterance; for non-silence data */
	while ((k = cont_ad_read (cont, buf, 4096)) == 0);
	if (k < 0)
	    E_FATAL("cont_ad_read failed\n");

	/* Non-silence data received; open and write to new logging file */
	uttno++;
	sprintf (file, "%04d.raw", uttno);
	if ((fp = fopen(file, "wb")) == NULL)
	    E_FATAL("fopen(%s,wb) failed\n", file);
	fwrite (buf, sizeof(int16), k, fp);
	uttlen = k;
	printf ("Utterance %04d, logging to %s\n", uttno, file);
	
	/* Note current timestamp */
	ts = cont->read_ts;
	
	/* Read utterance data until a gap of at least 1 sec observed */
	for (;;) {
	    if ((k = cont_ad_read (cont, buf, 4096)) < 0)
		E_FATAL("cont_ad_read failed\n");
	    if (k == 0) {
		/*
		 * No speech data available; check current timestamp.  End of
		 * utterance if no non-silence data been read for at least 1 sec.
		 */
		if ((cont->read_ts - ts) > endsilsamples)
		    break;
	    } else {
		/* Note timestamp at the end of most recently read speech data */
		ts = cont->read_ts;
		uttlen += k;
		fwrite (buf, sizeof(int16), k, fp);
	    }
	}
	fclose (fp);

	printf ("\tUtterance %04d = %d samples (%.1fsec)\n\n",
		uttno, uttlen, (double)uttlen/(double)sps);
    }

    ad_stop_rec (ad);
    cont_ad_close (cont);
    ad_close (ad);
    return 0;
}

