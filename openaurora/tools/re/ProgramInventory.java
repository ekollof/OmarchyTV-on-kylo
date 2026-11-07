// Dump a complete read-only inventory of one analyzed program.
//
// Usage:
//   analyzeHeadless <projdir> <project> -process <program> -readOnly -noanalysis \
//     -scriptPath tools/re -postScript ProgramInventory.java <outdir>
//
// Writes inventory.txt, functions.tsv, symbols.tsv, callgraph.tsv, data.tsv.
// This does not modify the program or create functions.
//@category tvhack
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;

public class ProgramInventory extends GhidraScript {
    private static String clean(String s) {
        if (s == null) return "";
        return s.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private static String preview(String s, int max) {
        if (s == null) return "";
        s = clean(s);
        return s.length() <= max ? s : s.substring(0, max) + "...";
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            throw new IllegalArgumentException("usage: ProgramInventory.java <outdir>");
        }
        File dir = new File(args[0]);
        if (!dir.isDirectory() && !dir.mkdirs()) {
            throw new IllegalStateException("cannot create " + dir);
        }
        String base = currentProgram.getName().replaceAll("[^A-Za-z0-9_.-]", "_");

        try (PrintWriter inv = new PrintWriter(new FileWriter(new File(dir, base + ".inventory.txt")));
             PrintWriter funcs = new PrintWriter(new FileWriter(new File(dir, base + ".functions.tsv")));
             PrintWriter syms = new PrintWriter(new FileWriter(new File(dir, base + ".symbols.tsv")));
             PrintWriter calls = new PrintWriter(new FileWriter(new File(dir, base + ".callgraph.tsv")));
             PrintWriter data = new PrintWriter(new FileWriter(new File(dir, base + ".data.tsv")))) {

            inv.println("program\t" + currentProgram.getName());
            inv.println("executable_path\t" + currentProgram.getExecutablePath());
            inv.println("executable_md5\t" + currentProgram.getExecutableMD5());
            inv.println("executable_sha256\t" + currentProgram.getExecutableSHA256());
            inv.println("language\t" + currentProgram.getLanguageID());
            inv.println("image_base\t" + currentProgram.getImageBase());
            inv.println("compiler\t" + currentProgram.getCompiler());
            inv.println("created\t" + currentProgram.getCreationDate());

            inv.println("\n[memory blocks]");
            long totalBytes = 0;
            for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
                long size = b.getSize();
                totalBytes += size;
                inv.printf("%s\t%s\t%s\t%d\t%s%s%s\t%s%n",
                        b.getName(), b.getStart(), b.getEnd(), size,
                        b.isRead() ? "r" : "-", b.isWrite() ? "w" : "-",
                        b.isExecute() ? "x" : "-",
                        b.isInitialized() ? "initialized" : "uninitialized");
            }
            inv.println("memory_bytes\t" + totalBytes);

            FunctionManager fm = currentProgram.getFunctionManager();
            int total = 0, external = 0, thunks = 0, named = 0;
            funcs.println("address\tsize\tname\tnamespace\tcallconv\tparams\treturn\tvarargs\tnoreturn\texternal\tthunk");
            for (Function f : fm.getFunctions(true)) {
                total++;
                if (f.isExternal()) external++;
                if (f.isThunk()) thunks++;
                String name = f.getName();
                String ns = f.getParentNamespace() == null ? "" : f.getParentNamespace().getName(true);
                if (!name.startsWith("FUN_") && !name.startsWith("thunk_FUN_")) named++;
                String cc = f.getCallingConventionName();
                funcs.printf("%s\t%d\t%s\t%s\t%s\t%d\t%s\t%s\t%s\t%s\t%s%n",
                        f.getEntryPoint(), f.getBody().getNumAddresses(), clean(name), clean(ns),
                        clean(cc), f.getParameterCount(),
                        clean(f.getReturnType() == null ? "" : f.getReturnType().getName()),
                        f.hasVarArgs(), f.hasNoReturn(), f.isExternal(), f.isThunk());
            }
            inv.println("functions_total\t" + total);
            inv.println("functions_external\t" + external);
            inv.println("functions_thunks\t" + thunks);
            inv.println("functions_named\t" + named);

            SymbolTable st = currentProgram.getSymbolTable();
            SymbolIterator si = st.getAllSymbols(true);
            int symCount = 0, extCount = 0;
            syms.println("address\ttype\tnamespace\tname\tsource\tprimary\texternal");
            while (si.hasNext()) {
                Symbol s = si.next();
                symCount++;
                if (s.isExternal()) extCount++;
                String addr;
                try {
                    addr = s.getAddress().toString();
                } catch (Exception e) {
                    addr = "?";
                }
                syms.printf("%s\t%s\t%s\t%s\t%s\t%s\t%s%n",
                        addr, s.getSymbolType(), clean(s.getParentNamespace().getName(true)),
                        clean(s.getName()), s.getSource(), s.isPrimary(), s.isExternal());
            }
            inv.println("symbols_total\t" + symCount);
            inv.println("symbols_external\t" + extCount);

            Listing listing = currentProgram.getListing();
            calls.println("call_site\tmnemonic\tcaller_entry\tcaller\tcallee_entry\tcallee\tflow");
            int edges = 0;
            InstructionIterator ii = listing.getInstructions(true);
            while (ii.hasNext()) {
                Instruction ins = ii.next();
                if (!ins.getFlowType().isCall()) continue;
                Function caller = getFunctionContaining(ins.getAddress());
                for (Address d : ins.getFlows()) {
                    Function callee = getFunctionAt(d);
                    if (callee == null) callee = getFunctionContaining(d);
                    if (callee == null) continue;
                    calls.printf("%s\t%s\t%s\t%s\t%s\t%s\t%s%n",
                            ins.getAddress(), ins.getMnemonicString(),
                            caller == null ? "" : caller.getEntryPoint().toString(),
                            caller == null ? "" : clean(caller.getName(true)),
                            callee.getEntryPoint(), clean(callee.getName(true)), "call");
                    edges++;
                }
            }
            inv.println("call_edges\t" + edges);

            data.println("address\ttype\tlength\tsymbol\tvalue");
            DataIterator di = listing.getDefinedData(true);
            int dataCount = 0;
            while (di.hasNext()) {
                Data d = di.next();
                if (d == null) continue;
                Symbol ps = st.getPrimarySymbol(d.getAddress());
                if (ps == null && !d.hasStringValue()) continue;
                String val = "";
                try {
                    val = d.hasStringValue() ? d.getValue().toString() : "";
                } catch (Exception e) {
                    val = "";
                }
                data.printf("%s\t%s\t%d\t%s\t%s%n",
                        d.getAddress(), clean(d.getDataType().getName()), d.getLength(),
                        ps == null ? "" : clean(ps.getName(true)), preview(val, 300));
                dataCount++;
            }
            inv.println("named_or_string_data\t" + dataCount);
        }
    }
}
