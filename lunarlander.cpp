// https://martincmartin.com/2024/06/14/how-i-found-a-55-year-old-bug-in-the-first-lunar-lander-game
// Based on: Translation of
// <http://www.cs.brandeis.edu/~storer/LunarLander/LunarLander/LunarLanderListing.jpg>
// by Jim Storer from FOCAL to C, with mods.
// See bottom comments for info about FOCAL and the FOCAL source.
// Variable names have been changed in part to aid in comprehending the code.
// Sometimes variables are calculated more than once, which is really for a debugging context, easy compare.
// As a C(++) program, many "improvements" are possible and for some, expected or even obligatory for some.
// However, I find the goto with loops structure quite interesting and do not want to put in anachronistic effort.
// Where I changed this (adding a break and continue in outer loop), it is not really much clearer,
// but it allows me to use a loop counter which then can be tested (additional output yes/no).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <io.h>
#include <ctype.h>
// sorry,  conio.h is windowy, not posix. Perhaps go for the clunkier push a key and/or ENTER.
// Work around if necessary.
#include <conio.h>
#ifdef _WIN32           // for retrieving parent process in waitkey()
 #include <process.h>
 #include <Windows.h>
 #include <tlhelp32.h>
 #include <psapi.h>
#endif
#include <cmath>
#include <vector>
#include <array>
#include <string>
#include <functional>
#include "brent.hpp"
template <typename fptype, typename func_type>
// numerical integration. Added, really, to check on my engine driven altitude gain integral, which should be exact.
// not needed in the program (anymore), but nice as a check.
double simpson_rule(fptype a, fptype b, int n, // Number of intervals
    func_type f)
{   // https://stackoverflow.com/questions/60005533/composite-simpsons-rule-in-c#61086158
    fptype h = (b - a) / n;
    // Internal sample points, there should be n - 1 of them
    fptype sum_odds = 0.0;
    for (int i = 1; i < n; i += 2) sum_odds += f(std::fma(i, h, a));
    fptype sum_evens = 0.0;
    for (int i = 2; i < n; i += 2) sum_evens += f(std::fma(i, h, a));
    return (std::fma(2, sum_evens, f(a)) + std::fma(4, sum_odds, f(b))) * h / 3;
}

// quadratic is a robust quadratic solver to find out whether numerical issues occur.
static bool quadratic(std::array<double, 2>& roots, double a, double b, double c)
{   // https://en.wikipedia.org/wiki/Quadratic_formula#Square_root_in_the_denominator and note 22
    // todo: complete using complex numbers. See commented lines, also at bottom of file.
    auto sgn = [](const double x) {return x > 0 ? 1 : (x < 0 ? -1 : 0); };
    if (a == 0) return false; const double b1 = b / a, c1 = c / a;
    if (b == 0) { roots = { -sqrt(-c1),  -roots[0] }; return true; } // ascending order
    if (c == 0) { roots = { -b1, 0 }; return true; }
    double y1 = 0, y2 = 0;
    const double c1abs = fabs(c1), scale = sqrt(c1abs) * sgn(b1), beta = b1 / (2 * scale), sc = sgn(c1);
    // if (isreal([b1,c1])
    {
        if (sc == -1) { y1 = beta + sqrt(beta * beta + 1); y2 = -1 / y1; }
        else if (beta >= 1)
        { y1 = beta + sqrt((beta + 1) * (beta - 1)); y2 = 1 / y1; }
        else return false;  // alleen complexe oplossingen.
        //else
        //{   // imaginair, zie gebruik j in complex.
        //    const auto im = sqrt((beta + 1) * (1 - beta));
        //    y1 = beta + j * im; y2 = beta - j * im;
        //}
    }
    //else
    //{   // with complex coefficients
    //    scale = sgn(b1) * (sqrt(abs(c1)));
    //    beta = abs(b1) / (2 * sqrt(abs(c1)));
    //    f = sqrt(sgn(c1)) / sgn(b1);
    //    gamma = sqrt((beta - f) * (beta + f));
    //    y1 = beta + sign(real(gamma)) * gamma;
    //    y2 = f ^ 2 / y1;
    //}
    roots = { -y1 * scale, -y2 * scale };
    if (roots[0] > roots[1]) std::swap(roots[0], roots[1]);
    return true;
}
static bool find_parentprocess(std::string& fname);

// Altitude (mi), Gravity constant, Mass (lbs), Velocity (mi/s), Time (s), Time in turn (s),
// Altitude at end of turn (mi), Speed at and of turn (mi/s), Time left in turn (s), Specific thrust (lbf/pound of fuel)
static double A, G, M, V, T, TF, X, EndAlt, EndSpeed, FR, EmptyMass, TimeRemain, SpecThrust;
#define Fuel (M - EmptyMass)
static bool echo_input = false, RedirectedInput = false;
enum calcmethod { ORIGINAL, BUGFIXED, EXACT, UNDECIDED };
static calcmethod CalcMethod{ UNDECIDED };


// calculate speed, altitude at end of (current part of) the current turn.
static void apply_thrust();
// finalize speed, altitude, mass to lander and update time and remaining time in turn (usually 0).
static void update_lander_state();

// Input routines (substitutes for FOCAL ACCEPT command).
static bool accept_double(double *value);
static int accept_yes_or_no();
static bool accept_line(char **buffer, int *buffer_length);
// Added key wait in case program is launched from e.g. explorer, not cmd etc, otherwise the ouput just disappears.
// Checks as best as possible whether this is needed (against twice press a key).
// The check is Windows only, since I could not find a portable CalcMethod for determining parent program (name) - and then, what.
#ifdef _WIN32
 static void waitkey();
#endif
// Optional arguments:
// --echo (see below)
// calc(CalcMethod)=[original|old || new|fixed || exact], default original. see message.

static double getalt(const double t)
{ return A - 0.5 * G * t * t - V * t - SpecThrust * ((t - M / FR) * log(1 - t * FR / M) - t); };

static void telwhat(const char *argv0)
{
    const char* fn = strrchr(argv0, '\\');
    if (!fn) fn = strrchr(argv0, '/');
    if (!fn) fn = argv0; else ++fn;
    fputs(fn, stdout);
    if (strncmp(fn, "lunarlander", 11))
        fputs(", aka lunarlander", stdout);
    puts(
        "written originally by Jim Storer in 1969,\n"
        "shortly after the first moon landing, using Focal on a PDP8\n"
        "and ported later to basic and C.\n"
        "Lunarlander was by far the most famous and popular early computergame.\n"
        "It is also quite hard to achieve a good landing by simply guessing inputs\n"
        "The rise of modern OS'es, starting with unix, aided us by redirection, so we can\n"
        "fire off more variations in input without having to press the keys each time.\n"
        "Part of the difficulty of the game may result from slight inaccuracies in the\n"
        "thrust application and more so, in the calculation of the lowest point when\n"
        "there is enough thrust to cause ascending.\n"
        "This happens occasionally and then a landing is assumed when the lowest\n"
        "calculated altitude is less than or equal to zero, which occurs occasionally.\n"
        "The more common case is a landing without such a speed reversal.\n"
        "M. C. Martin published a blog to discuss a bug in the lowest point calculation.\n"
        "In this code, the corrected code is present and can be used optionally.\n"
        "Then we discussed whether a lowest point above but close to the surface can or\n"
        "even should be regarded as a landing, especially when gravity (after engine\n"
        "switch-off) would lead to a good or even perfect landing.\n"
        "Calculation method can be specified by calc=[original|bugfix|exact]\n"
        "(default original).\n"
        "Exact means using the rocket equation with a logarithm for thrust application\n"
        "and a primitive for the altitude calculation, rather than Taylor terms.\n"
        "As in other ports, --echo prints input, which is useful with redirected input.\n"
        "An additional output has been added at speed-reversal. Altitude is shown signed\n"
        "to allow for a value in feet which is zero after rounding, but can be positive\n"
        "causing a (temporary) fly-off and a subsequent hard landing.\n"
        "Just to be clear: this code is meant for fun and experimentation.\n"
        "The code serves no useful purpose, is free to use and modify and does not\n"
        "require mention of this work.\n"
        "See the code for links to the blog and the C-port used here as beginning.\n"
    );
}

int main(int argc, char **argv)
{
    int turn = 0;
    const char* calcmess = "original";   // default
    bool dohelp = false;
    for (int ia = 1; ia < argc; ++ia)
    {   // If --echo is present, then write all input back to standard output.
        // (This is useful for testing with files as (redirected) input.)
        char * arg = _strlwr(argv[ia]);
        if (strchr("-/", arg[0]))
        {
            while (*arg && !isalnum(*arg)) ++ arg;
            if (*arg == 'h' || *arg == '?') dohelp = true;
        }
        if (!echo_input) echo_input = strcmp(arg, "--echo") == 0;
        char* equals{ nullptr };
        if (CalcMethod == UNDECIDED && !strncmp(arg, "calc", 4) && (equals = strchr(arg, '=')) != nullptr)
        {
            if (strstr(equals, "old") || strstr(equals,"orig")) { CalcMethod = ORIGINAL; calcmess = "original"; }
            else if (strstr(equals, "new") || strstr(equals,"fixed") || !strncmp(equals,"bugfix",6))
            { CalcMethod = BUGFIXED; calcmess = "bugfixed"; }
            else if (strstr(equals, "exact")) { CalcMethod = EXACT; calcmess = "exact"; }
            else { printf("Do not understand %s\n", arg); return 1; }
        }
    }
    if (CalcMethod == UNDECIDED) CalcMethod = ORIGINAL;
    if (!dohelp) printf("Using the %s version for time to lowest point (zero speed)\n", calcmess);
    RedirectedInput = !_isatty(_fileno(stdin));
    if (RedirectedInput) echo_input = true;

    puts("CONTROL CALLING LUNAR MODULE. MANUAL CONTROL IS NECESSARY");
    puts("YOU MAY RESET FUEL RATE FR EACH 10 SECS TO 0 OR ANY VALUE");
    puts("BETWEEN 8 & 200 LBS/SEC. YOU'VE 16000 LBS FUEL. ESTIMATED");
    puts("FREE FALL IMPACT TIME-120 SECS. CAPSULE WEIGHT-32500 LBS\n\n");
    if (dohelp)
    {
        telwhat(argv[0]);
        return 0;
    }
    do // 01.20 in original FOCAL code
    {
        puts("FIRST RADAR CHECK COMING UP\n\n");
        puts("COMMENCE LANDING PROCEDURE");
        puts("TIME,SECS   ALTITUDE,MILES+FEET   VELOCITY,MPH   FUEL,LBS   FUEL RATE");

        A = 120;
        V = 1;
        M = 32500;
        EmptyMass = 16500;
        G = .001;
        SpecThrust = 1.8;
        T = 0;

    start_turn: // 02.10 in original FOCAL code
        printf("%7.0f%16.0f%7.0f%15.2f%12.1f      ", T, trunc(A), 5280 * (A - trunc(A)), 3600 * V, Fuel);
        ++turn;

    prompt_for_k:
        fputs("FR:=", stdout);
        const auto accepted = accept_double(&FR);
        if (!accepted || FR < 0 || (0 < FR && FR < 8) || FR > 200)
        { fputs("NOT POSSIBLE", stdout); for (int x = 1; x <= 51; ++x) putchar('.'); goto prompt_for_k; }
        if (RedirectedInput) putchar('\n');

        TimeRemain = 10;

    //turn_loop:
        for (int il31 = 0;;++il31) // 03.10 in original FOCAL code
        {
            if (Fuel < .001) goto fuel_out;
            if (TimeRemain < .001) goto start_turn;
            // Additional output coming in well when having a flyoff or, contrarily, a landing when close to ground.
            if (il31) printf("%11.3f%12.0f%+7.0f%15.2f%12.1f      FR  %.0lf\n", T, trunc(A), 5280 * (A - trunc(A)), 3600 * V, Fuel, FR);
            TF = TimeRemain;
            if (TF * FR > Fuel) TF = Fuel / FR;

            apply_thrust();

            if (EndAlt <= 0)
                goto loop_until_on_the_moon;

            if (V > 0 && EndSpeed < 0)
            {   // can only get here with power (FR) during the landing turn resulting in negative acceleration.
                for (int il81 = 0;;++il81) // 08.10 in original FOCAL code
                {
                    // FOCAL-to-C gotcha: In FOCAL, multiplication has a higher // precedence than division.
                    // In C, they have the same precedence and are evaluated left-to-right.
                    // So the original FOCAL subexpression `M * G / SpecThrust * FR` can't be copied as-is
                    // into C: `SpecThrust * FR` has to be parenthesized to get the same result.

                    // TF becomes time to zero speed -> time to lowest point given the motion direction reversal.
                    // you might try with the simplest estimate of TF for V == 0
                    // const auto acc = G - SpecThrust * FR / M;
                    // TF = -V / acc;    // which really comes out too high, overshoot, no obvious iteration available.
                    X = 0.5 * (1 - M * G / (SpecThrust * FR));
#                 ifdef _DEBUG
                    // precalculate TF according to the old formula for comparison to bugfix and other attempts
                    // You may want to leave out the addition of 0.05 sec, or apply it also in the bugfix.
                    TF = M * V / (SpecThrust * FR * (X + sqrt(X * X + V / SpecThrust))) + 0.05;
#                 else
                    if (CalcMethod == ORIGINAL) TF = M * V / (SpecThrust * FR * (X + sqrt(X * X + V / SpecThrust))) + 0.05;
#                 endif
                    if (CalcMethod == BUGFIXED)   // if modern, overwrite TF with corrected formula
                    {
#                     ifdef _DEBUG
                        // try other solution. Didn't work sofar, consider deprecated.
                        TF = M * V / (SpecThrust * FR * (X - sqrt(X * X + 0.5 * V / SpecThrust)));
#                     endif
                        TF = M * V / (SpecThrust * FR * (X + sqrt(X * X + 0.5 * V / SpecThrust)));
                    }
                    else if (CalcMethod == EXACT)
                         TF = brent::zero(0, TF, 1e-9, getalt);		// 3rd parameter is tolerance.

                    apply_thrust();
                    // choose between original <= 0 or <= small value which may lead to a good landing instead of an flyoff.
                    //if ((CalcMethod == ORIGINAL && EndAlt <= 0) || (CalcMethod >= BUGFIXED && EndAlt <= 0.00003858))
                    if (EndAlt <= 0.00003858)
                    {   // a perfect landing to be expected by turning of the engine at (very) low EndAlt.
                        // This also relieves small inaccuracies in the TF calculation.
                        if (EndAlt >= 0)     // but smaller than or equal to 0.00003858
                        {   // just let it go, baby. It will be ok.
                            // loop_until_on_the_moon may fail to converge (really a marginal fly-off).
                            TF = sqrt(2 * EndAlt / G);
                            V = EndSpeed = TF * G;
                            A = EndAlt = 0;
                            T += TF;
                            goto on_the_moon;
                        }
                        goto loop_until_on_the_moon; // lowest point under surface, find conditions when hitting ground.
                    }
                    update_lander_state();
                    // this condition is confused. May be the EndSpeed test should be done before update_lander_state()
                    // The EndSpeed and V comparisons are separate in the original code 8.30 (J and V).
                    //if (EndSpeed > 0 || V <= 0) goto turn_loop;     // V is set equal to EndSpeed in update_lander_state() ?!!
                    //if (EndSpeed > 0 || V <= 0) break;     // to continue the 3.10 loop, with loopcounter increased, not reset.
                    // if time has not run out, we want to repeat the search for negative altitude rather than repeating apply_thrust()
                    if (TimeRemain < 0.001 || V <= 0) break;  // force reading a new input if time has run out and apply_thrust() anyway.
                }
                continue;       // avoids update_lander_state() (again). Used to be goto turn_loop, now loop with counter.
            }
            update_lander_state();
        }
        // The way here appears to be make an estimate without mass change (apply_thrust), then apply_thrust and
        // hopefully not keep undershooting the surface. The equation is 0.5Gt2 + Vt = altitude (to be lost),
        // Some have mentioned a possible numerical problem (catastrophic cancellation).
        // I've added a robust quadratic solver, after which I cannot yet conclude to any such problem,
        // but given the sometimes very low and close values, it is certainly possible.
        // See https://en.wikipedia.org/wiki/Quadratic_formula#Square_root_in_the_denominator and note 22.
        // A potential improvement is to check whether start or finish altitude of the turn is closer to zero
        // to minimize the approximation effort, but it does not appear needed.
    loop_until_on_the_moon: // 07.10 in original FOCAL code
        while (TF >= .005)
        {   // calculate time from level zero to underground (A), reduce speed (marginal), update (landing)time, mass.
            std::array<double, 2> roots;
            // TF should be pretty much equal to 5 or 6 digits or more in various way of calculating it.
            // original formula, ok and still effectively used after precalculating acceleration and discriminant.
            // TF = 2 * A / (V + sqrt(V * V + 2 * A * (G - SpecThrust * FR / M)));
            const auto acc = G - SpecThrust * FR / M, disc = sqrt(V * V + 2 * A * acc);
            TF = (disc - V) / acc;  // usual formula. See whether numerator and denominator root choice may differ explaining choice.
#         ifdef _DEBUG
            const auto tf = 2 * A / (disc + V);     // equivalent to commented original formula
            if (tf != TF && fabs(tf - TF) > 1e-9) fprintf(stderr, "%.10lf vs %.10lf\n", tf, TF);
#         endif
            TF = 2 * A / (disc + V); // discriminant in denominator. This is expected to be consistently right.
            // If we calculate undershoot correction, A should be positive -> negative in quadratic equation (sidechange).
            if (CalcMethod == EXACT && quadratic(roots, 0.5 * acc, V, -A))     // return false in case of no real root(s)
                TF = roots[roots[0] < 0];
            if (TF > 0) apply_thrust();
            else if (TF < 0) { EndSpeed += TF * acc; EndAlt = 0; TF = 0; }  // not expected.
            update_lander_state();
        }
        goto on_the_moon;

    fuel_out: // 04.10 in original FOCAL code
        printf("\nFUEL OUT AT %8.2f SECS\n", T);
        TF = (sqrt(V * V + 2 * A * G) - V) / G;
        V += G * TF;
        T += TF;

    on_the_moon: // 05.10 in original FOCAL code
        printf("\nON THE MOON AT   %8.3f SECS\n", T);
        X = 3600 * V;
        printf("IMPACT VELOCITY: %8.3f M.P.H.\n", X);
        printf("FUEL LEFT:       %8.2f LBS\n", Fuel);
        if (X <= 1) puts("PERFECT LANDING !-(LUCKY)");
        else if (X <= 10) puts("GOOD LANDING-(COULD BE BETTER)");
        else if (X <= 22) puts("CONGRATULATIONS ON A POOR LANDING");
        else if (X <= 40) puts("CRAFT DAMAGE. GOOD LUCK");
        else if (X <= 60) puts("CRASH LANDING-YOU'VE 5 HRS OXYGEN");
        else
        {
            puts("SORRY,BUT THERE WERE NO SURVIVORS-YOU BLEW IT!");
            printf("IN FACT YOU BLASTED A BUGFIXED LUNAR CRATER %8.2f FT. DEEP\n", X * .277777);
        }

        if (!RedirectedInput) puts("\nTRY AGAIN?"); else putchar('\n');
    } while (accept_yes_or_no() == 1);

    puts("CONTROL OUT");
    waitkey();
    return 0;
}

// Subroutine at line 06.10 in original FOCAL code
static void update_lander_state()
{
    T += TF;
    TimeRemain -= TF;
    M -= TF * FR;
    A = EndAlt;
    V = EndSpeed;
}

// Subroutine at line 09.10 in original FOCAL code
static void apply_thrust()
{
    const double Q = TF * FR / M, Q_2 = Q * Q, Q_3 = Q_2 * Q, Q_4 = Q_3 * Q, Q_5 = Q_4 * Q;

    const double endspeedExact = V + G * TF + SpecThrust * log(1 - Q);        // exact, for comparison
    // Using Taylor expansion, NB deltax is negative -> terms get the same sign, no sign altercation:
    EndSpeed = V + G * TF + SpecThrust * (-Q - Q_2 / 2 - Q_3 / 3 - Q_4 / 4 - Q_5 / 5);
    double endaltExact = A - G * TF * TF / 2 - V * TF;
    if (Q > 0)
    {
        const auto a = FR / M;
        // a bit of simpson to integrate to distance (altitude) increase.
        //auto lfunc = [a](const double t) { return log(1 - a * t); };
        //const auto y = simpson_rule<double, decltype(lfunc)>(0., TF, 10, lfunc);
        const auto z = (TF - 1 / a) * log(1 - Q) - TF;
        endaltExact -= SpecThrust * z;       // exact.
    }
    // Taylor expansion integrated (t = 0 to TF), sum dA for gravity, starting speed and engine.
    EndAlt = A - G * TF * TF / 2 - V * TF + SpecThrust * TF * (Q / 2 + Q_2 / 6 + Q_3 / 12 + Q_4 / 20 + Q_5 / 30);
    if (CalcMethod == EXACT) { EndSpeed = endspeedExact; EndAlt = endaltExact; }
}

// Read a floating-point value from stdin.
// Returns 1 on success, or 0 if input did not contain a number.
// Calls exit(-1) on EOF or other failure to read input.
static bool accept_double(double *value)
{
    char *buffer = NULL;
    int buffer_length = 0;
    if (accept_line(&buffer, &buffer_length))
    {
        int is_valid_input = sscanf(buffer, "%lf", value);
        free(buffer);
        return is_valid_input == 1;
    }
    return false;
}

// Reads input and returns 1 if it starts with 'Y' or 'y', or returns 0 if it
// starts with 'EmptyMass' or 'n'.
// If input starts with none of those characters, prompt again.
// If unable to read input, calls exit(-1);
static int accept_yes_or_no()
{
    if (RedirectedInput) return 0;
    int result = -1;
    do
    {
        fputs("(ANS. YES OR NO):", stdout);
        char *buffer = NULL;
        int buffer_length = 0;
        if (!accept_line(&buffer, &buffer_length)) return -1;

        if (buffer_length > 0)
        {
            switch (buffer[0])
            {
            case 'y':
            case 'Y':
            case 'j':
            case 'J':
                result = 1;
                break;
            case 'n':
            case 'N':
                result = 0;
                break;
            default:
                break;
            }
        }

        free(buffer);
    } while (result < 0);
    putchar('\n');
    return result;
}

static int getline(char** buffer, int* buflen, FILE* stream)
{
    // Zie https ://stackoverflow.com/questions/58670828/is-there-a-way-to-rewind-stdin-in-c. stdin niet seekable.
    if (!stream || feof(stream) || !buffer || !buflen) return -1;
    int bufsiz = 256;
    *buffer = (char*)malloc(bufsiz);
    if (!*buffer) return -1;
    char* ptr = fgets(*buffer, bufsiz, stream);
    *buflen = 0;
    if (!ptr) return -1;
    else if (!*ptr) return 0;
    return *buflen = (int)strlen(*buffer);
    //return *buflen < bufsiz 1 || (*buffer)[*buflen 1] == '\n' ? *buflen : -1;
}

static void waitkey()
{   // ideally, use parent-process (or similar means) to know whether the window will be closed on exit,
    // loosing the output before it can be inspected;
    // then do the press a key routine. Can't do that portably I'm afraid.
#  ifdef _WIN32
    std::string parname;
    if (find_parentprocess(parname))
    {   // cmd.exe, powershell zijn mogelijk, vermoedelijk is alleen explorer een directe sluiter.
//#     ifdef _DEBUG
//        fprintf(stderr, "Parent proces: %s\n", parname.c_str());
//#     endif
    }
    // msvsmon.exe is the remote visual studio debugger, used for x86 and behaving differently from VsDebugConsole.exe
    if ((parname == "explorer.exe" || parname.empty() || parname == "msvsmon.exe") && parname != "VsDebugConsole.exe")   // "msvsmon.exe"
#  endif
    { int r; fputs("Press a key", stderr); while (_kbhit()) r=_getch(); r=_getch(); }
}

// Reads a line of input.  Caller is responsible for calling free() on the returned buffer.
// If unable to read input, calls exit(-1).
static bool accept_line(char **buffer, int *buffer_length)
{
    if (getline(buffer, buffer_length, stdin) == -1) { fputs("\nEND OF INPUT\n", stderr); waitkey(); exit(-1); }
    auto e = strlen(*buffer);
    while (e > 0 && isspace((*buffer)[--e])) (*buffer)[e] = 0;
    if (echo_input) fputs(*buffer,stdout);
    return true;
}

/*
* https://en.wikipedia.org/wiki/FOCAL_(programming_language)
* uses linenumbers consisting of groupnumber.sub(line)number.
* The group number can be adressed by a single digit when it is printed as 01 for example.
* 7.1 in code seems to also refer to 7.10 (7.1 not to be found otherwise). Abbrevation is key for terseness.
* I : IF. IF calculates a num expr and can be followed by max 3 linenumbers separated by ',', for negative, zero resp. positive.
* D : DO. DO can get a group (no dot) and executes the group, or a full linenumber, then executes only the line.
* F : FOR, perhaps (also) function?
* G : GOTO
* R : RETURN
* S : SET (assignments)
* T : TYPE (print)
* Variable names (do not start with F): any length, only first two characters are used. So DES means the same as DE.
 Original global variables

 A Altitude (miles)
 G Gravity
 I Intermediate altitude (miles)
 J Intermediate velocity (miles/sec)
 K Fuel rate (lbs/sec)
 L Elapsed time (sec)
 M Total weight (lbs)
 N Empty weight (lbs, Note: M N is remaining fuel weight)
 TF Time elapsed in current 10-second turn (sec)
 T Time remaining in current 10-second turn (sec)
 V Downward speed (miles/sec)
 X Temporary working variable
 Z Thrust (in LBF) per pound of fuel burned

01.04 T "CONTROL CALLING LUNAR MODULE. MANUAL CONTROL IS NECESSARY"!
01.06 T "YOU MAY RESET FUEL RATE K EACH 10 SECS TO 0 OR ANY VALUE"!
01.08 T "BETWEEN 8 & 200 LBS/SEC. YOU'VE 16000 LBS FUEL. ESTIMATED"!
01.11 T "FREE FALL IMPACT TIME-120 SECS. CAPSULE WEIGHT-32500 LBS"!
01.20 T "FIRST RADAR CHECK COMING UP"!!!;E
01.30 T "COMMENCE LANDING PROCEDURE"!"TIME,SECS   ALTITUDE,"
01.40 T "MILES+FEET   VELOCITY,MPH   FUEL,LBS   FUEL RATE"!
01.50 S A=120;S V=1;S M=32500;S N=16500;S G=.001;S Z=1.8

02.10 T "    ",%3,L,"       ",FITR(A),"  ",%4,5280*(A-FITR(A))
02.20 T %6.02,"       ",3600*V,"    ",%6.01,M-N,"      K=";A K;S T=10
02.70 T %7.02;I (200-K)2.72;I (8-K)3.1,3.1;I (K)2.72,3.1
02.72 T "NOT POSSIBLE";F X=1,51;T "."
02.73 T "K=";A K;G 2.7

03.10 I (M-N-.001)4.1;I (T-.001)2.1;S S=T
03.40 I ((N+S*K)-M)3.5,3.5;S S=(M-N)/K
03.50 D 9;I (I)7.1,7.1;I (V)3.8,3.8;I (J)8.1
03.80 D 6;G 3.1

04.10 T "FUEL OUT AT",L," SECS"!
04.40 S S=(FSQT(V*V+2*A*G)-V)/G;S V=V+G*S;S L=L+S

05.10 T "ON THE MOON AT",L," SECS"!;S W=3600*V
05.20 T "IMPACT VELOCITY OF",W,"M.P.H."!,"FUEL LEFT:"M-N," LBS"!
05.40 I (1-W)5.5,5.5;T "PERFECT LANDING !-(LUCKY)"!;G 5.9
05.50 I (10-W)5.6,5.6;T "GOOD LANDING-(COULD BE BETTER)"!;G 5.9
05.60 I (22-W)5.7,5.7;T "CONGRATULATIONS ON A POOR LANDING"!;G 5.9
05.70 I (40-W)5.81,5.81;T "CRAFT DAMAGE. GOOD LUCK"!;G 5.9
05.81 I (60-W)5.82,5.82;T "CRASH LANDING-YOU'VE 5 HRS OXYGEN"!;G 5.9
05.82 T "SORRY,BUT THERE WERE NO SURVIVORS-YOU BLEW IT!"!"IN "
05.83 T "FACT YOU BLASTED A BUGFIXED LUNAR CRATER",W*.277777," FT.DEEP"!
05.90 T !!!!"TRY AGAIN?"!
05.92 A "(ANS. YES OR NO)"P;I (P-0NO)5.94,5.98
05.94 I (P-0YES)5.92,1.2,5.92
05.98 T "CONTROL OUT"!!!;Q

06.10 S L=L+S;S T=T-S;S M=M-S*K;S A=I;S V=J

07.10 I (S-.005)5.1;S S=2*A/(V+FSQT(V*V+2*A*(G-Z*K/M)))
07.30 D 9;D 6;G 7.1

08.10 S W=(1-M*G/(Z*K))/2;S S=M*V/(Z*K*(W+FSQT(W*W+V/Z)))+.05;D 9
08.30 I (I)7.1,7.1;D 6;I (-J)3.1,3.1;I (V)3.1,3.1,8.1

09.10 S Q=S*K/M;S J=V+G*S+Z*(-Q-Q^2/2-Q^3/3-Q^4/4-Q^5/5)
09.40 S I=A-G*S*S/2-V*S+Z*S*(Q/2+Q^2/6+Q^3/12+Q^4/20+Q^5/30)
*/
/* quadratics, robuust
beta=[];e=[];scale=[];
% special cases of zero elements
if a==0, return, else b1=b/a;c1=c/a; end
if b==0, x1=sqrt(-c1);x2=-x1; return, end
if c==0, x1=-b1; x2=0; return, end
% generic case
if isreal([b1,c1]),
% with real coefficients
c1abs=abs(c1);
scale=sqrt(c1abs)*sign(b1);
beta=b1/(2*scale);
e=sign(c1);
% computing the roots
if e==-1, y1=beta+sqrt(beta^2+1);y2=-1/y1;
else,
if beta >= 1
y1=beta+sqrt((beta+1)*(beta-1));
y2=1/y1;
else
im=sqrt((beta+1)*(1-beta));
y1=beta+j*im;y2=beta-j*im;
end
end
else,
% with complex coefficients
scale=sign(b1)*(sqrt(abs(c1)));
beta=abs(b1)/(2*sqrt(abs(c1)));
f=sqrt(sign(c1))/sign(b1);
gamma=sqrt((beta-f)*(beta+f));
y1=beta+sign(real(gamma))*gamma;
y2=f^2/y1;
end
x1=-y1*scale;x2=-y2*scale;*/

#ifdef _WIN32
static bool find_parentprocess(std::string& fname)
{
    static PROCESSENTRY32 PE32;
    PE32.dwSize = sizeof(PROCESSENTRY32);
    bool found = false;
    char buf[2048];
    fname.clear();
    const HANDLE proc = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (proc == INVALID_HANDLE_VALUE) return false;
    if (!Process32First(proc, &PE32)) return false;
    do
    {
        if (PE32.th32ProcessID == _getpid()) {
            const HANDLE parproc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, PE32.th32ParentProcessID);
            if (parproc)
            {
                const auto anslen = GetModuleBaseName(parproc, NULL, buf, sizeof(buf));
                CloseHandle(parproc);
                fname = std::string(buf, anslen);
                found = true; break;
            }
        }
    } while (Process32Next(proc, &PE32));
    CloseHandle(proc);
    return found;
}
#endif