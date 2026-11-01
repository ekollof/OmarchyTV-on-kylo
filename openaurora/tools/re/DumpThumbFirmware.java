// Explicit Thumb entrypoints missed by a partial analysis. Use -readOnly
// -noanalysis: temporary context/disassembly/function metadata is discarded.
// Never clear existing code or redefine an existing function. Addresses are
// actual aligned Thumb entrypoints established from instructions/pointer tables.
//@category tvhack
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.math.BigInteger;

public class DumpThumbFirmware extends GhidraScript {
    @Override
    public void run() throws Exception {
        println("PROGRAM " + currentProgram.getName() + " MD5 " + currentProgram.getExecutableMD5());
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException("decompiler open failed");
        try {
            for (String query : getScriptArgs()) {
                Address address = toAddr(query.startsWith("@") ? query.substring(1) : query);
                if ((address.getOffset() & 1) != 0) throw new IllegalArgumentException("Use aligned Thumb entrypoint");
                Function function = getFunctionContaining(address);
                if (function == null) {
                    if (getInstructionAt(address) == null) {
                        currentProgram.getProgramContext().setValue(
                            currentProgram.getRegister("TMode"), address, address, BigInteger.ONE);
                        if (!disassemble(address)) throw new IllegalStateException("disassembly failed: " + address);
                    }
                    function = createFunction(address, "review_" + address);
                    if (function == null) throw new IllegalStateException("function creation failed: " + address);
                }
                println("FUNCTION " + query + " at " + function.getEntryPoint() + " " + function.getName());
                DecompileResults result = decompiler.decompileFunction(function, 120, monitor);
                if (!result.decompileCompleted() || result.getDecompiledFunction() == null)
                    throw new IllegalStateException("decompilation failed: " + result.getErrorMessage());
                println(result.getDecompiledFunction().getC());
                var instructions = currentProgram.getListing().getInstructions(function.getBody(), true);
                while (instructions.hasNext()) {
                    monitor.checkCancelled();
                    var instruction = instructions.next();
                    println(instruction.getAddress() + " " + instruction);
                }
            }
        } finally {
            decompiler.dispose();
        }
    }
}
