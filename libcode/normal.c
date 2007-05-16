/*
 * Mathematical functions, which should be usable on any C99
 * platform using IEEE double precision, dealing with erf and the
 * normal distribution.
 * 
 * This file provides the inverse functions for erf and erfc (but
 * relies on a C99-compliant libm to provide the forward
 * functions), and also provides forward and inverse functions for
 * the cumulative probability of the standard normal distribution
 * (with the forward functions implemented in terms of erf and
 * erfc).
 */

/*
 * This is not the exact value of sqrt(2); it's the exact value of
 * the double-precision number which best approximates sqrt(2).
 */
static const double sqrt2 =
    1.41421356237309492343001693370752036571502685546875;

/*
 * Compute the inverse of erf(x) on the interval [0,1/2]. 
 */
static double internal_inverf(double x)
{
    /*
     * Rational-function approximation to erf^-1(x)/x on [0,1/2].
     * Chosen to minimise the absolute error when using x*R to
     * approximate erf^-1(x), which is in the region of 1.803e-17.
     */

    double R =
	(24.79891658395006602700752 + x *
	 (-26.52635103384296143068798 + x *
	  (-17.85810303277236842617414 + x *
	   (22.8587626983257943607289 + x *
	    (-0.5074692037602971426958849 + x *
	     (-2.776933661208881431905754 + x *
	      (0.4049114938195780283526959 + x *
	       (-0.07055737814581092921659848)))))))) /
	(27.98258083986896288839596 + x *
	 (-29.93178188569917931793058 + x *
	  (-27.47653395615149002951928 + x *
	   (33.62947370592647835963619 + x *
	    (2.593142223250945639233033 + x *
	     (-7.629492654451346583181029 + x))))));

    return x * R;
}

/*
 * Compute the inverse of erfc(x) on the interval [0,1/2].
 */
static double internal_inverfc(double x)
{
    if (x >= 0.0625) {
	/*
	 * Rational-function approximation to erfc^-1(x)/x on
	 * [1/16,1/2]. Chosen to minimise the absolute error,
	 * which is in the region of 6.354e-18.
	 */

	double R =
	    (6.284896345197495558914915e-7 + x *
	     (0.0002577706562380964540010194 + x *
	      (0.02171627440943737650223214 + x *
	       (0.5994343192985166803686503 + x *
		(6.214364923561344146875889 + x *
		 (22.97630938478630941255748 + x *
		  (14.96546442106547243010588 + x *
		   (-37.01638747310264847513728 + x *
		    (-18.56336812766293229150158 + x *
		     (10.53743323023856072904245 + x *
		      (0.2648027830515574706759769))))))))))) /
	    (2.556586132314347240329708e-7 + x *
	     (0.0001291489824420862308603112 + x *
	      (0.01287643023540355677181042 + x *
	       (0.4243915015475855750650172 + x *
		(5.503107620656153774579376 + x *
		 (29.11336129036108144362395 + x *
		  (56.63105655514271194404333 + x *
		   (19.85383430129493305366594 + x *
		    (-20.79897356647317849288248 + x *
		     (-2.864571681563122371751799 + x))))))))));

	return R;
    } else {
	/*
	 * For large x, we observe that erfc(x) is very similar to
	 * e^-x^2, and hence for small x erfc^-1(x) is very
	 * similar to sqrt(-log(x)). So the most sensible way to
	 * compute erfc^-1 on [0,1/16] is to actually _compute_
	 * sqrt(-log(x)), and then apply a rational function
	 * approximation which gives us erfc(x) in terms of that.
	 * 
	 * Our approximation is therefore to erfc^-1(exp(-x^2)),
	 * and it's valid on the interval [sqrt(-log(1/16)),30].
	 * (30 is actually rather higher than the square root of
	 * the negative log of the smallest IEEE double denormal;
	 * that's about 27.2.)
	 * 
	 * The approximation is constructed to minimise absolute
	 * error from that function, and its absolute error is in
	 * the region of 3.175e-18.
	 */
	double y = sqrt(-log(x));
	double R =
	    (-349366007580.4958895573563 + y *
	     (9043414967534.380502796925 + y *
	      (1857822948916374.626592314 + y *
	       (4812123485476295.702116669 + y *
		(7292899729204705.017000335 + y *
		 (8569045805509224.581248963 + y *
		  (4275366175771342.90991371 + y *
		   (1340599960363150.11997151 + y *
		    (297156676500564.4624472177 + y *
		     (35208799518987.69892507065 + y *
		      (1856733892503.900506111505 + y *
		       (38269583498.1045351023821 + y *
			(230688308.5925711650525207))))))))))))) /
	    (2172630249358955.7478382 + y *
	     (5131191834319428.430450343 + y *
	      (10037123376125059.64844583 + y *
	       (11119213378955259.95291082 + y *
		(9919885878857128.212683678 + y *
		 (4633106880931229.423154537 + y *
		  (1395304430324187.739548646 + y *
		   (300834561033734.0249471817 + y *
		    (35304304741929.59260956419 + y *
		     (1857487692619.404790811993 + y *
		      (38269978560.88646946230056 + y *
		       (230687605.1269198485907946 + y))))))))))));

	return R;
    }
}

/*
 * Compute the full inverse of erf().
 */
double inverf(double d)
{
    /*
     * Range check.
     */
    if (-1.0 >= d || +1.0 <= d) {
	volatile double zero = 0.0;
	errno = EDOM;
	return zero/zero;	       /* NaN */
    }

    /*
     * Split into half-integer intervals.
     */
    if (d < -0.5)
	return -internal_inverfc(d + 1.0);
    else if (d < 0)
	return -internal_inverf(-d);
    else if (d <= 0.5)
	return internal_inverf(d);
    else /* d > 0.5 */
	return internal_inverfc(1.0 - d);
}

/*
 * Compute the full inverse of erfc().
 */
double inverfc(double d)
{
    /*
     * Range check.
     */
    if (0.0 >= d || +2.0 <= d) {
	volatile double zero = 0.0;
	errno = EDOM;
	return zero/zero;	       /* NaN */
    }

    /*
     * Split into half-integer intervals.
     */
    if (d < 0.5)
	return internal_inverfc(d);
    else if (d <= 1.0)
	return internal_inverf(1.0 - d);
    else if (d <= 1.5)
	return -internal_inverf(d - 1.0);
    else /* d > 0.5 */
	return -internal_inverfc(2.0 - d);
}

/*
 * Compute the cumulative distribution function for the standard
 * normal distribution.
 */
double Phi(double x)
{
    return -0.5 * erfc(x/sqrt2);
}

/*
 * Same as Phi, but offset so that Phi0(0)=0. In other words,
 * instead of being the integral of the normal distribution from
 * -infinity to x, this is the integral from _zero_ to x.
 */
double Phi0(double x)
{
    return 0.5 * erf(x/sqrt2);
}

/*
 * Compute the inverse of Phi.
 */
double invPhi(double x)
{
    return sqrt2 * inverfc(-2.0 * x);
}

/*
 * Compute the inverse of Phi0.
 */
double invPhi0(double x)
{
    return sqrt2 * inverf(2.0 * x);
}
