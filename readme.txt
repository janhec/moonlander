A version of the famous Jim Storer moonlander game, reflecting attention, explanation, praise and a bugfix by Martin C. Martin,
with comments.
Please peruse https://martincmartin.com/2024/06/14/how-i-found-a-55-year-old-bug-in-the-first-lunar-lander-game.
In this version:
- the bugfix is optionally used;
- exact calculation is added as an option;
- one more potential bug is discussed;
- an option to avoid fly-offs when within good landing distance comes up.
- slight errors in the original C translation of the FOCAL source are discussed.
- The project is a MSVC project and should compile well as such.
- Linux users should remove some stuff which in windows prevents immediate window closure
  when started from e.g. explorer, avoiding having more than one pres a key moment.
  Prevents a small irritation but immaterial otherwise.
  Can be compiled by enumerating the .cpp files, compiling and linking with standard libs.
- contains in commented form the FOCAL source and a little bit of explanation of FOCAL (WIKI)
  to help reading the source.
- moonlander.c is the translation, with obvious mistakes in translation corrected.
  For reference, not in the project.
- inputsui.txt (and inputsui0.txt) are input files reflecting Martin's suicide burn(s).
  The 0.txt has the pre-bugfix optimum suicide burn, the .txt has the one achieved afterwards.

So, the idea is that you can go on where I left off or even did ridiculous things, whatever.
Have fun.

Update 21-06-2024
- added optional calculation methods: original, bugfixed, exact.
- Applied exact method (optionally) to apply_thrust() as well.
- used low positive altitude tolerance for switch off for all methods
- changed continuation of low positive altitude tolerance to (goto) on_the_moon
  instead of (goto) loop_until_on_the_moon.
The latter may want to fly off and produce ugly results;
the point is going straight down with engine switched off if the drop is small enough.
The idea is then to measure this so accurately that impact velocity can be less than
0.01 MPH owing to the minimal (or no) fall at the end of the last full-burn turn.
- changed inputsui.txt to inputsuicideburns.txt with accurate inputs for all calculation methods.