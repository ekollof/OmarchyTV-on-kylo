// Recover immediate argument values at every call to a named symbol.
//
// Usage:
//   analyzeHeadless <projdir> <project> -process <program> -readOnly -noanalysis \
//     -scriptPath tools/re -postScript DumpCallArgs.java <symbol> <reg> <outfile>
//
// <reg> is r0..r3 (default r0). Scans backward within the same basic block
// (stops at a call/branch/function boundary), handles MOV/MOVS/MOVW/MOVT and
// prints UNKNOWN when the value is not a local immediate. Read-only.
//@category tvhack
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;
import java.io.FileWriter;
import java.io.PrintWriter;

public class DumpCallArgs extends GhidraScript {
    private static String clean(String s) {
        return s == null ? "" : s.replace('\t', ' ').replace('\n', ' ');
    }

    private static boolean isReg(Instruction ins, int opIndex, String reg) {
        Object[] ops = ins.getOpObjects(opIndex);
        return ops.length >= 1 && ops[0] instanceof Register
                && ((Register) ops[0]).getName().equalsIgnoreCase(reg);
    }

    private static Long scalar(Instruction ins, int opIndex) {
        Object[] ops = ins.getOpObjects(opIndex);
        if (ops.length >= 1 && ops[0] instanceof Scalar) {
            return ((Scalar) ops[0]).getUnsignedValue();
        }
        return null;
    }

    /** Returns the immediate written to reg by this instruction, or null. */
    private static Long assigned(Instruction ins, String reg, Long[] movtHigh) {
        String m = ins.getMnemonicString().toUpperCase();
        if (!isReg(ins, 0, reg)) return null;
        Long v = scalar(ins, 1);
        if (v == null) return null;
        if (m.equals("MOVT")) {
            movtHigh[0] = v;
            return null;
        }
        if (m.equals("MOVW")) {
            long lo = v;
            long hi = movtHigh[0] == null ? 0 : movtHigh[0];
            movtHigh[0] = null;
            return (hi << 16) | lo;
        }
        if (m.equals("MOV") || m.equals("MOVS")) {
            movtHigh[0] = null;
            return v;
        }
        return null;
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 3) {
            throw new IllegalArgumentException("usage: DumpCallArgs.java <symbol> <reg> <outfile>");
        }
        String symbol = args[0];
        String reg = args[1].toLowerCase();
        PrintWriter out = new PrintWriter(new FileWriter(args[2]));
        out.println("symbol\tcall_site\tcaller_entry\tcaller\tvalue\tstatus\tcontext");

        int calls = 0, known = 0;
        for (Instruction ins : currentProgram.getListing().getInstructions(true)) {
            if (!ins.getFlowType().isCall()) continue;
            boolean match = false;
            for (Address d : ins.getFlows()) {
                Function f = getFunctionAt(d);
                if (f == null) f = getFunctionContaining(d);
                if (f != null && f.getName(true).contains(symbol)) {
                    match = true;
                    break;
                }
            }
            if (!match) continue;
            calls++;

            Function caller = getFunctionContaining(ins.getAddress());
            Long found = null;
            Long movtHigh = null;
            StringBuilder ctx = new StringBuilder();
            Instruction p = ins.getPrevious();
            for (int n = 0; n < 16 && p != null; n++) {
                if (caller != null && getFunctionContaining(p.getAddress()) != caller) break;
                if (p.getFlowType().isCall() || p.getFlowType().isJump()
                        || p.getFlowType().isTerminal()) {
                    break;
                }
                if (ctx.length() > 0) ctx.insert(0, " ; ");
                ctx.insert(0, p.getAddress() + ": " + clean(p.toString()));
                Long v = assigned(p, reg, new Long[] { movtHigh });
                if (v != null) {
                    found = v;
                    break;
                }
                p = p.getPrevious();
            }
            if (found != null) known++;
            out.printf("%s\t%s\t%s\t%s\t%s\t%s\t%s%n", symbol, ins.getAddress(),
                    caller == null ? "" : caller.getEntryPoint().toString(),
                    caller == null ? "" : clean(caller.getName(true)),
                    found == null ? "" : String.format("0x%x", found),
                    found == null ? "UNKNOWN" : "IMM",
                    clean(ctx.toString()));
        }
        out.printf("# calls=%d known=%d%n", calls, known);
        out.close();
        println("DumpCallArgs " + symbol + " reg=" + reg + " calls=" + calls
                + " known=" + known + " -> " + args[2]);
    }
}
