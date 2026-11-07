// Dump defined strings and the functions that reference them.
//
// Usage:
//   analyzeHeadless <projdir> <project> -process <program> -readOnly -noanalysis \
//     -scriptPath tools/re -postScript DumpStringRefs.java <out.tsv> [regex]
//
// Optional regex filters the string value (java.util.regex). Output columns:
// string_address, string, reference_address, function_entry, function.
// Read-only; no decompilation.
//@category tvhack
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.regex.Pattern;

public class DumpStringRefs extends GhidraScript {
    private static String clean(String s) {
        return s == null ? "" : s.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            throw new IllegalArgumentException("usage: DumpStringRefs.java <out.tsv> [regex]");
        }
        Pattern pat = args.length > 1 ? Pattern.compile(args[1]) : null;
        Listing listing = currentProgram.getListing();
        ReferenceManager refs = currentProgram.getReferenceManager();
        PrintWriter out = new PrintWriter(new FileWriter(args[0]));
        out.println("string_address\tstring\treference_address\tfunction_entry\tfunction");

        int strings = 0, rows = 0;
        DataIterator di = listing.getDefinedData(true);
        while (di.hasNext()) {
            Data d = di.next();
            if (d == null) continue;
            String s;
            try {
                Object v = d.getValue();
                if (!(v instanceof String)) continue;
                s = (String) v;
            } catch (Exception e) {
                continue;
            }
            if (pat != null && !pat.matcher(s).find()) continue;
            strings++;
            for (Reference r : refs.getReferencesTo(d.getAddress())) {
                Address from = r.getFromAddress();
                Function f = getFunctionContaining(from);
                out.printf("%s\t%s\t%s\t%s\t%s%n", d.getAddress(), clean(s), from,
                        f == null ? "" : f.getEntryPoint().toString(),
                        f == null ? "" : clean(f.getName(true)));
                rows++;
            }
        }
        out.printf("# strings=%d refs=%d%n", strings, rows);
        out.close();
        println("DumpStringRefs strings=" + strings + " refs=" + rows + " -> " + args[0]);
    }
}
