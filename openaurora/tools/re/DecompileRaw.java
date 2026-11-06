// Disassemble/create a function at an exact address and decompile it. Intended
// for Thumb task entries that Ghidra did not discover as functions.
// Run on a disposable project copy: it creates the function in that copy.
//@category tvhack
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import java.math.BigInteger;

public class DecompileRaw extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface decomp = new DecompInterface();
        if (!decomp.openProgram(currentProgram))
            throw new IllegalStateException("Cannot open program");
        for (String query : getScriptArgs()) {
            Address a = toAddr(query);
            Register tmode = currentProgram.getProgramContext().getRegister("TMode");
            if (tmode != null)
                currentProgram.getProgramContext().setValue(tmode, a, a, BigInteger.ONE);
            Function f = getFunctionAt(a);
            if (f == null) {
                DisassembleCommand dis = new DisassembleCommand(a, null, true);
                dis.applyTo(currentProgram, monitor);
                CreateFunctionCmd create = new CreateFunctionCmd(a);
                create.applyTo(currentProgram, monitor);
                f = getFunctionAt(a);
            }
            if (f == null) {
                println("NO FUNCTION at " + a);
                continue;
            }
            DecompileResults r = decomp.decompileFunction(f, 60, monitor);
            if (!r.decompileCompleted()) {
                println("DECOMPILE FAILED at " + a + ": " + r.getErrorMessage());
                continue;
            }
            println("===== " + a + " " + f.getName(true) + " =====");
            println(r.getDecompiledFunction().getC());
        }
    }
}
