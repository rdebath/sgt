/*
 * Efficient and yet accurate implementation of the Lambert W
 * function on the reals in IEEE double precision. As best I can
 * tell, seems accurate to about 3-4 ULP everywhere.
 */

/*
 * The Lambert W function is the inverse of f(x) = x e^x. This
 * inverse is multi-valued on (-1/e,0), with one value greater
 * than -1 and one less; the `principal branch' is generally
 * defined as the value greater than or equal to -1.
 * 
 * So the principal branch of W(x), implemented below by the
 * function W(), has no value at all for x < -1/e; it assumes the
 * value 1 at x=-1/e and proceeds upwards with infinite
 * derivative; it goes through (0,0) and thereafter grows more
 * slowly than log(x).
 * 
 * The non-principal branch, implemented below by the function
 * Wn(), is only defined on [-1/e,0); it assumes the value 1 at
 * x=-1/e and proceeds downwards with infinite derivative, and
 * ends up tending to -infinity as x approaches zero.
 * 
 * Readers who wish to verify these basic properties of W might
 * find the following gnuplot implementation useful (it is slow
 * and has low accuracy but is basically the right shape):

f(x) = x*exp(x)
wb(x, lo, hi) = abs((hi-lo)/(hi+lo)) < 1e-14 ? (lo+hi)/2 : f((lo+hi)/2) < x ? wb(x, (lo+hi)/2, hi) : wb(x, lo, (lo+hi)/2)
wbn(x, lo, hi) = abs((hi-lo)/(hi+lo)) < 1e-14 ? (lo+hi)/2 : f((lo+hi)/2) > x ? wbn(x, (lo+hi)/2, hi) : wbn(x, lo, (lo+hi)/2)
W(x) = x < -exp(-1) ? 1/0.0 : wb(x, -1.0, log(1+x)+1)
Wn(x) = x < -exp(-1) || x >= 0.0 ? 1/0.0 : wbn(x, -755.0, -1.0)

set xrange [-0.4:4]
set yrange [-4:2]
plot W(x), Wn(x)

 * W is directly the inverse of x e^x, but it can also be used to
 * invert other related functions:
 *
 *  - if we need to solve x log x = y, then we can substitute x =
 *    e^z to turn this into z e^z = y, and hence deduce z = W(y)
 *    and so x = e^W(y). For -1/e < y < 0 there is a second
 *    solution using Wn.
 * 
 *  - if we have x + log x = y, then we can raise e to the power
 *    of both sides to obtain x e^x = e^y, and so x = W(e^y).
 *    Since e^y is always positive, this gives a single solution
 *    for any positive real.
 * 
 *  - if we have x^x = y, this means e^(x log x) = y, whence x log
 *    x = log y and hence by the above we have x = e^W(log y). For
 *    -1/e < log y < 0 there is a second solution.
 */

#include <math.h>
#include <errno.h>

double W(double x)
{
    /*
     * Range check against -1/e.
     * 
     * The constant here is _not_ the true mathematical value of
     * -1/e. It is the true mathematical value, to 25 places, of
     * the largest exactly representable IEEE 754 number which is
     * smaller than -1/e. In other words, it is the first number
     * for which W has no value.
     */
    if (x <= -0.3678794411714423340242774) {
	volatile double zero = 0.0;
	errno = EDOM;
	return zero/zero;	       /* NaN */
    }

    /*
     * Split into subintervals.
     */

    if (x >= -0.125 && x <= 1.0) {
	/*
	 * On the interval [-1/8,+1] we simply approximate W(x) as
	 * x*R(x), where R is a rational-function approximation to
	 * W(x)/x. (Doing this instead of directly finding an
	 * approximation to W(x) has the effect of forcing the
	 * first term of the function's numerator to be zero,
	 * which ensures full precision down to extremely small
	 * values of x.)
	 */
	double R =
	    (9.029872804969977696957285 + x *
	     (79.82916689280599414318019 + x *
	      (278.2001954368822527181674 + x *
	       (485.9088378991319368153246 + x *
		(447.3747930629345551590066 + x *
		 (209.2087416012807727523787 + x *
		  (43.64734423380342435341441 + x *
		   (2.940558965170632669029582 + x *
		    (0.0145813959593915124836176))))))))) /
	    (9.029872804969977626653517 + x *
	     (88.85903969777601221757941 + x *
	      (353.51442592720369894731 + x *
	       (730.2143650928992551236033 + x *
		(837.2443709331261161400459 + x *
		 (528.5518285736738913887819 + x *
		  (171.2427611307651914681538 + x *
		   (24.18976514064451500000319 + x))))))));

	return x * R;
    } else if (x < 0.0) {
	/*
	 * On the interval [-1/e,-1/8] we need to avoid the
	 * infinite derivative displayed by W at -1/e, so we
	 * don't want to directly approximate W. If we instead
	 * approximate W(x^2-1/e), the derivative becomes
	 * nicely finite again. And if we add one to that as
	 * well, then it goes through (0,0) which means we can
	 * get accurate answers very near the limit of the
	 * range. Finally, we observe that this function goes
	 * through zero, and divide out the x term to force it
	 * to really be zero at zero; this seems to improve
	 * accuracy at the extreme low end.
	 *
	 * Before we can apply this approximation, we must
	 * first compute y such that y^2-1/e equals our input
	 * x. In order to do this accurately, we provide two
	 * floating-point constants which sum to 1/e at very
	 * high precision.
	 */
	double xx1 = x + 0.3678794411714418899892777;   /* this is exact */
	double xx2 = xx1 + 4.31660456177274253001730784275563e-16;
	double xx3 = sqrt(xx2);
	double F =
	    (69.48191866020896726247865 + xx3 *
	     (286.1347350008990513459132 + xx3 *
	      (427.1496121153159260085585 + xx3 *
	       (280.2894139704694045821195 + xx3 *
		(77.31362012046225710711524 + xx3 *
		 (6.786689359084433397154738 + xx3 *
		  (0.04360520674809448833192791))))))) /
	    (29.7995402422523357711796 + xx3 *
	     (145.8786605116897182542852 + xx3 *
	      (271.8247046459699084674172 + xx3 *
	       (240.3919492152864694795322 + xx3 *
		(102.2740740101975526615421 + xx3 *
		 (18.59768127286852896873422 + xx3))))));

	return F*xx3 - 1.0;
    } else /* x >= 1.0 */ {
	/*
	 * For x above 1, we take y=log(x), and then develop
	 * approximations for W(e^y).
	 */
	double y = log(x);

	if (y <= 10) {
	    /*
	     * Approximation for W(e^y) on [0,10].
	     */
	    double W =
		(-4986469650616.322779304023 + y *
		 (-8856993504840.602126328107 + y *
		  (-7915142879823.277258017099 + y *
		   (-4599355382042.08553289055 + y *
		    (-1903145682945.464140970047 + y *
		     (-581571990889.1861013587394 + y *
		      (-132173596963.3707997219607 + y *
		       (-21955024742.31883673230554 + y *
			(-2539681938.258944520680338 + y *
			 (-184457095.1944842152391953 + y *
			  (-6657317.324655129793786032 + y *
			   (-75022.0356652136912047732)))))))))))) /
		(-8792257150769.424803050265 + y *
		 (-10006480990906.74359485648 + y *
		  (-6428782142651.782920355074 + y *
		   (-2728330611526.887311429066 + y *
		    (-828602758105.9961222412266 + y *
		     (-182757042851.8442046522929 + y *
		      (-28963749563.16609754418733 + y *
		       (-3145645149.552009802379287 + y *
			(-211353262.1586951072901603 + y *
			 (-7063996.765300293180821511 + y *
			  (-75452.12019556553687115759 + y)))))))))));

	    return W;
	} else if (y <= 100) {
	    /*
	     * Approximation for W(e^y) on [10,100], found by
	     */
	    double W =
		(-40553477473955668629.77298 + y *
		 (-54882573795272046363.99124 + y *
		  (-35716744322833350636.50822 + y *
		   (-12898700225785309715.23188 + y *
		    (-3009381645905610950.829311 + y *
		     (-316122484908228110.6256337 + y *
		      (-14507978007606416.4962229 + y *
		       (-297346364256428.7866827596 + y *
			(-2655861399110.008931620565 + y *
			 (-9274871222.092115527141907 + y *
			  (-9242111.374479126012624829))))))))))) /
		(-71089145539741732460.83141 + y *
		 (-52198935050433062830.73626 + y *
		  (-19705786080343854076.4448 + y *
		   (-3977332181396146482.005325 + y *
		    (-370822387815734819.61938 + y *
		     (-15840344537855171.93004354 + y *
		      (-311340095498739.5189136073 + y *
		       (-2713831944724.024149888933 + y *
			(-9346952383.73281487514291 + y *
			 (-9246901.69220461509361523 + y))))))))));
	    return W;
	} else /* y >= 100 */ {
	    /*
	     * Approximation for W(e^y) on [100,710] (this is far
	     * enough, since e^710 is outside IEEE double range),
	     * found by:
	     */

	    double W =
		(-7.92302833109544582652143e+28 + y *
		 (-4.764719070508289834186225e+28 + y *
		  (-2.849382017869899984772914e+28 + y *
		   (-2.376368684417795945210506e+27 + y *
		    (-6.504921771538721474485398e+25 + y *
		     (-740928619454218206936116.7 + y *
		      (-3849596171625253368102.133 + y *
		       (-9322036975913919995.76114 + y *
			(-10146381469629431.76656633 + y *
			 (-4413511889715.37766908884 + y *
			  (-556666725.3480407434467691))))))))))) /
		(-1.122517002555802472490792e+29 + y *
		 (-3.665465187203671355566957e+28 + y *
		  (-2.654243749397491405857361e+27 + y *
		   (-6.876121185878554610601095e+25 + y *
		    (-762960943546664811389878.1 + y *
		     (-3909820515119805545739.759 + y *
		      (-9395882960812643763.39601 + y *
		       (-10182945883824881.78144873 + y *
			(-4418996435832.414438198527 + y *
			 (-556703807.6229043341104785 + y))))))))));
	    return W;
	}
    }
}

double Wn(double x)
{
    /*
     * Range check against -1/e and 0.
     * 
     * Same constant as in W() above; same comments apply.
     */
    if (x <= -0.3678794411714423340242774 || x >= 0.0) {
	volatile double zero = 0.0;
	errno = EDOM;
	return zero/zero;	       /* NaN */
    }

    /*
     * Split into subintervals.
     */

    if (x < -0.25) {
	/*
	 * As above in W(), on the interval [-1/e,-1/4] we
	 * approximate Wn(x^2-1/e) in order to avoid infinite
	 * derivatives.
	 */
	double xx1 = x + 0.3678794411714418899892777;   /* this is exact */
	double xx2 = xx1 + 4.3166045617727425300173078e-16;
	double xx3 = sqrt(xx2);
	double F =
	    (18.87284105182103161762024 + xx3 *
	     (-112.8418405717615208501989 + xx3 *
	      (267.2985173060989471691065 + xx3 *
	       (-317.1446610513608558455072 + xx3 *
		(195.3207040023797669065265 + xx3 *
		 (-57.7508818037130384337725 + xx3 *
		  (6.268460922520730446678862 + xx3 *
		   (-0.06367132583406110459250275)))))))) /
	    (-8.094220730427960008595246 + xx3 *
	     (54.68677479474910873109058 + xx3 *
	      (-150.4199230876901278213115 + xx3 *
	       (215.6743828424590534922867 + xx3 *
		(-171.0122125582951272873253 + xx3 *
		 (72.94247290955311902840503 + xx3 *
		  (-14.78350830352546114027647 + xx3)))))));

	return F*xx3 - 1.0;
    } else /* -1/4 <= x < 0.0 */ {
	/*
	 * On the remaining interval, we take logs (as we did for
	 * the infinite interval of W()), and find approximations
	 * for Wn(-e^y) where y = log(-x). Thus the useful range
	 * of y is [-745,log(1/4)].
	 */
	double y = log(-x);

	if (y <= -100) {
	    /*
	     * An approximation to Wn(-e^y) on [-745,-100].
	     */
	    double W =
		(-3.055827598594469889548451e+28 + y *
		 (1.555421294728908911693922e+29 + y *
		  (-4.2170651642550914027537e+28 + y *
		   (2.947004755737550413445539e+27 + y *
		    (-7.572840852852843236686212e+25 + y *
		     (837123017401330113358140.3 + y *
		      (-4268207179288852283342.929 + y *
		       (10177317030077872155.54941 + y *
			(-10911455306730061.68351201 + y *
			 (4671025350704.175867708357 + y *
			  (-578979592.0374976752846061))))))))))) /
		(8.438521074196199068306635e+28 + y *
		 (-3.360318329832405312627154e+28 + y *
		  (2.651430531277487541336465e+27 + y *
		   (-7.173552908361964865077158e+25 + y *
		    (813322286643558116339123.6 + y *
		     (-4203284634009731120461.812 + y *
		      (10098291501097558679.13267 + y *
		       (-10872779110477871.55212379 + y *
			(4665311977495.453588766297 + y *
			 (-578941707.2091116081218752 + y))))))))));

	    return W;
	} else if (y <= -10) {
	    /*
	     * An approximation to Wn(-e^y) on [-100,-10].
	     */
	    double W =
		(-3789501314066018470043779.0 + y *
		 (-1.200736303762380923912534e+25 + y *
		  (1.90505492136392978504263e+25 + y *
		   (1.407247596280484020674585e+25 + y *
		    (-1.888472760873979274054881e+25 + y *
		     (6449459554173563038126696.0 + y *
		      (-994608877123727484302372.7 + y *
		       (79850007066809906653265.45 + y *
			(-3532934018096633413824.241 + y *
			 (87812949013832894906.68607 + y *
			  (-1217453941352309214.062396 + y *
			   (9102962870450252.285672795 + y *
			    (-34197096048923.74186208982 + y *
			     (56070606831.02637553088745 + y *
			      (-28637001.70253794067641204))))))))))))))) /
		(-1497138663423324108611045.0 + y *
		 (8741705846765707532600386.0 + y *
		  (4.45399738831735541672022e+24 + y *
		   (-1.101562028862124124384865e+25 + y *
		    (4584363754959048672803409.0 + y *
		     (-794065503482745341921393.7 + y *
		      (68756288623337563602845.1 + y *
		       (-3201721448590447847826.892 + y *
			(82428887627529812605.79731 + y *
			 (-1170873069267543083.061392 + y *
			  (8901148678101662.040460734 + y *
			   (-33811435423106.88738838856 + y *
			    (55829593122.82891255712414 + y *
			     (-28628781.12854967912195065 + y))))))))))))));

	    return W;
	} else {
	    /*
	     * An approximation to Wn(-e^y) on [-10,log(1/4)].
	     */
	    double W =
		(-4386805158.601123266755079 + y *
		 (-28423649807.41514597360483 + y *
		  (19944540408.75710487773951 + y *
		   (350381730602.1350088917267 + y *
		    (516157717832.8241641283608 + y *
		     (-218447024456.018969871281 + y *
		      (-790179406914.4822143343103 + y *
		       (-145618048874.875898209377 + y *
			(320864611824.7880607744242 + y *
			 (36124848637.19894090265883 + y *
			  (-56602603630.18310642789655 + y *
			   (11007307396.55165037217362 + y *
			    (-778814683.4578785284807083 + y *
			     (20571703.44753648336823214 + y *
			      (-154355.6677616425612659653))))))))))))))) /
		(208483847.645650545268414 + y *
		 (21123840946.48072209011934 + y *
		  (89959728731.3472877826076 + y *
		   (46421103797.63774393739605 + y *
		    (-255031611303.9676756096363 + y *
		     (-328244687798.2877124992249 + y *
		      (63888583265.55148860965522 + y *
		       (199369874740.5646903579901 + y *
			(-6543171310.094599166306908 + y *
			 (-36685914173.25240815832327 + y *
			  (8810949335.305143779556298 + y *
			   (-696610151.2817829990460453 + y *
			    (19688859.1522960071696555 + y *
			     (-153731.0982126324566733901 + y))))))))))))));

	    return W;
	}
    }
}

#ifdef TEST_W

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    char buf[512];

    while (fgets(buf, sizeof(buf), stdin)) {
	double x, y;

	x = strtod(buf, NULL);
	y = W(x);
	printf("W = %.24g (%.24g)\n", y, y*exp(y));
	y = Wn(x);
	printf("Wn = %.24g (%.24g)\n", y, y*exp(y));
    }
}

#endif
