// Decompile exact symbols/addresses, optionally print disassembly, or list
// matching function names with find:<regex> without changing the program.
// Run with analyzeHeadless -readOnly -noanalysis -scriptPath tools/re
//     -postScript DumpFirmware.java [--asm] <symbol-or-@hex-address> ...
//     -postScript DumpFirmware.java 'find:(?i)(cec|audio)'
//     -postScript DumpFirmware.java data:006aaf88:0xb8
// This script does not change the program or infer missing function signatures.
//@category tvhack
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.regex.Pattern;

public class DumpFirmware extends GhidraScript {
    @Override
    public void run() throws Exception {
        println("PROGRAM " + currentProgram.getName() + " MD5 " + currentProgram.getExecutableMD5());
        DecompInterface decomp = new DecompInterface();
        if (!decomp.openProgram(currentProgram)) {
            throw new IllegalStateException("Cannot open program for decompilation");
        }
        boolean assembly = false;
        try {
            for (String query : getScriptArgs()) {
                if (query.equals("--asm")) {
                    assembly = true;
                    continue;
                }
                if (query.startsWith("data:")) {
                    String[] parts = query.split(":", -1);
                    if (parts.length != 3) throw new IllegalArgumentException("Use data:hex-address:length");
                    int length = Integer.decode(parts[2]);
                    if (length < 1 || length > 65536) throw new IllegalArgumentException("Data length must be 1..65536");
                    byte[] bytes = new byte[length];
                    var address = toAddr(parts[1]);
                    if (currentProgram.getMemory().getBytes(address, bytes) != length)
                        throw new IllegalArgumentException("Incomplete data read: " + query);
                    println("DATA " + address + " length " + length);
                    for (int offset = 0; offset < length; offset += 16) {
                        StringBuilder row = new StringBuilder(address.add(offset).toString()).append("  ");
                        StringBuilder ascii = new StringBuilder();
                        for (int i = offset; i < Math.min(length, offset + 16); i++) {
                            int value = Byte.toUnsignedInt(bytes[i]);
                            row.append(String.format("%02x ", value));
                            ascii.append(value >= 32 && value <= 126 ? (char)value : '.');
                        }
                        println(row.append(" ").append(ascii).toString());
                    }
                    continue;
                }
                if (query.startsWith("find:")) {
                    Pattern pattern = Pattern.compile(query.substring(5));
                    FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
                    while (functions.hasNext()) {
                        monitor.checkCancelled();
                        Function f = functions.next();
                        if (pattern.matcher(f.getName(true)).find())
                            println("MATCH " + f.getEntryPoint() + " " + f.getName(true));
                    }
                    continue;
                }
                Set<Function> found = new LinkedHashSet<>();
                if (query.startsWith("@")) {
                    Function f = getFunctionContaining(toAddr(query.substring(1)));
                    if (f != null) found.add(f);
                } else {
                    SymbolIterator symbols = currentProgram.getSymbolTable().getSymbols(query);
                    while (symbols.hasNext()) {
                        Symbol s = symbols.next();
                        Function f = getFunctionAt(s.getAddress());
                        if (f != null) found.add(f);
                    }
                }
                if (found.isEmpty()) {
                    throw new IllegalArgumentException("No function for exact query: " + query);
                }
                for (Function f : found) {
                    monitor.checkCancelled();
                    println("\n===== " + query + " -> " + f.getName(true) + " @ " + f.getEntryPoint());
                    println("Signature (analysis metadata, verify against instructions): " + f.getPrototypeString(true, false));
                    DecompileResults result = decomp.decompileFunction(f, 120, monitor);
                    if (!result.decompileCompleted() || result.getDecompiledFunction() == null) {
                        throw new IllegalStateException("Decompile failed: " + result.getErrorMessage());
                    }
                    println(result.getDecompiledFunction().getC());
                    if (assembly) {
                        println("DISASSEMBLY " + f.getEntryPoint());
                        InstructionIterator insns = currentProgram.getListing().getInstructions(f.getBody(), true);
                        while (insns.hasNext()) {
                            Instruction insn = insns.next();
                            println(insn.getAddress() + "  " + insn);
                        }
                    }
                }
            }
        } finally {
            decomp.dispose();
        }
    }
}
