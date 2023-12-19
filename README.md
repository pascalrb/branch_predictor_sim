# Branch Predictor Simulator

This simulator implements a branch predictor which can then be used to evaluate different configurations of branch predictors.
Predictors: bimodal, gshare, hybrid

Guided by a project from ECE563 at NC State University taught by Prof. [Eric Rotenberg](https://ece.ncsu.edu/people/ericro/).

# Instructions on Using the Simulator
1. Type "make" to build.  (Type "make clean" first if you already compiled and want to recompile from scratch.)

2. Run trace reader:
   To run without throttling output:
   ```
   ./bp_sim bimodal 6 traces/gcc_trace.txt
   ./bp_sim gshare 9 3 traces/gcc_trace.txt
   ./bp_sim hybrid 8 14 10 5 traces/gcc_trace.txt
   ```

   To run with throttling (via "less"):
   ```
   ./bp_sim bimodal 6 traces/gcc_trace.txt | less
   ./bp_sim gshare 9 3 traces/gcc_trace.txt | less
   ./bp_sim hybrid 8 14 10 5 traces/gcc_trace.txt | less
   ```
