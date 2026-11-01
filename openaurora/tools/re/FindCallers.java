// List references (call sites and data reads) to one or more symbols or addresses
// without changing the program. This is the fast path for "who calls X" questions:
// Ghidra's reference manager already holds the xrefs, so there is no need to dump
// and grep the whole ELF.
//
// Run with analyzeHeadless -readOnly -noanalysis -scriptPath openaurora/tools/re
//     -postScript FindCallers.java <symbol-or-@hex> [...]
//     -postScript FindCallers.java _ZN11UIFramework30setFullScreenAnimationProgressEf
//     -postScript FindCallers.java @0x113bd48
// Only call references can be requested with --calls; the default lists every
// reference type (call, data read/write) because a "caller" may be a vtable or
// a function pointer store, which objdump call-site greps miss entirely.
//@category tvhack
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public class FindCallers extends GhidraScript {
    @Override
    public void run() throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();
        SymbolTable st = currentProgram.getSymbolTable();
        boolean callsOnly = false;
        List<String> queries = new ArrayList<>();
        for (String arg : getScriptArgs()) {
            if (arg.equals("--calls")) {
                callsOnly = true;
            } else {
                queries.add(arg);
            }
        }
        if (queries.isEmpty()) {
            println("usage: FindCallers.java [--calls] <symbol-or-@hex> [...]");
            return;
        }
        println("PROGRAM " + currentProgram.getName() + " MD5 " + currentProgram.getExecutableMD5());
        for (String query : queries) {
            println("");
            println("===== " + query);
            Set<Address> targets = new LinkedHashSet<>();
            if (query.startsWith("@")) {
                targets.add(toAddr(query.substring(1)));
            } else {
                SymbolIterator it = st.getSymbols(query);
                while (it.hasNext()) {
                    targets.add(it.next().getAddress());
                }
            }
            if (targets.isEmpty()) {
                println("  no symbol matches " + query);
                continue;
            }
            for (Address target : targets) {
                Function targetFn = fm.getFunctionAt(target);
                println("  target " + target + "  " +
                        (targetFn == null ? "(not a function entry)" : targetFn.getName()));
                List<Reference> refs = new ArrayList<>();
                ReferenceIterator it = rm.getReferencesTo(target);
                while (it.hasNext()) {
                    Reference ref = it.next();
                    if (callsOnly && !ref.getReferenceType().isCall()) continue;
                    refs.add(ref);
                }
                refs.sort(Comparator.comparing(Reference::getFromAddress));
                if (refs.isEmpty()) {
                    println("    no " + (callsOnly ? "call " : "") + "references");
                    continue;
                }
                println("    " + refs.size() + (callsOnly ? " call" : "") + " reference(s)");
                for (Reference ref : refs) {
                    Address from = ref.getFromAddress();
                    Function caller = fm.getFunctionContaining(from);
                    Instruction insn = getInstructionAt(from);
                    println(String.format("    %s  %-14s  %-40s  %s",
                            from,
                            ref.getReferenceType(),
                            caller == null ? "(no function)" : caller.getName(),
                            insn == null ? "" : insn.toString()));
                }
            }
        }
    }
}
