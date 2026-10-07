package org.supermanreturns.mobile;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.List;

final class GameFiles {
    static final String XEX_SHA256 = "c8f243acd99de9a91f5ae4f409721c0e954e3d5eb96861419d3da07b8106db2b";
    static final List<String> ARCHIVES = List.of("_workinp", "baseworl", "fmv", "fmvlegal", "lunits",
        "metropol", "objtiv", "preload", "sound", "text", "ui", "voice");
    private static final long[] ARCHIVE_SIZES = {46047,6253440,996058192,42352054,25696316,
        245672344,349035444,2122295,305930445,233532,89595420,87061326};

    static String canonical(String path) {
        String lower = path.toLowerCase(java.util.Locale.ROOT);
        if (lower.equals("default.xex")) return "default.xex";
        for (String name : ARCHIVES) if (lower.equals("data/" + name + ".ast")) return "DATA/" + name + ".AST";
        return null;
    }

    static void validate(Path root) throws IOException {
        if (!Files.isRegularFile(root.resolve("default.xex")) || !sha256(root.resolve("default.xex")).equals(XEX_SHA256))
            throw new IOException("default.xex incompatível. Use a edição Xbox 360 suportada de Superman Returns (454107ED / 64A4002A).");
        for (int i=0;i<ARCHIVES.size();i++) {
            String name=ARCHIVES.get(i);
            Path p = root.resolve("DATA/" + name + ".AST");
            if (!Files.isRegularFile(p) || Files.size(p) != ARCHIVE_SIZES[i]) throw new IOException("Arquivo ausente ou tamanho incompatível: DATA/" + name + ".AST");
        }
    }

    static String sha256(Path path) throws IOException {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            try (InputStream input = Files.newInputStream(path)) {
                byte[] buffer = new byte[1024 * 1024]; int n;
                while ((n = input.read(buffer)) != -1) digest.update(buffer, 0, n);
            }
            StringBuilder hex = new StringBuilder(64);
            for (byte b : digest.digest()) hex.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
            return hex.toString();
        } catch (NoSuchAlgorithmException e) { throw new AssertionError(e); }
    }
}
