package org.supermanreturns.mobile;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.Comparator;

/** Commit only after full validation; interrupted promotion is recovered on next startup. */
final class InstallStore {
    final Path root, active, staging, backup;
    InstallStore(Path root) { this.root=root; active=root.resolve("game"); staging=root.resolve("game-import"); backup=root.resolve("game-backup"); }
    void recover() throws IOException {
        Files.createDirectories(root);
        if (!Files.exists(active) && Files.exists(backup)) Files.move(backup, active, StandardCopyOption.ATOMIC_MOVE);
        if (Files.exists(active) && Files.exists(backup)) delete(backup);
    }
    void prepare() throws IOException { recover(); delete(staging); Files.createDirectories(staging); }
    void commit() throws IOException {
        GameFiles.validate(staging);
        if (Files.exists(active)) Files.move(active, backup, StandardCopyOption.ATOMIC_MOVE);
        try { Files.move(staging, active, StandardCopyOption.ATOMIC_MOVE); }
        catch (IOException e) { if (Files.exists(backup)) Files.move(backup, active, StandardCopyOption.ATOMIC_MOVE); throw e; }
        // The committed installation is valid even if backup cleanup fails.
        try { delete(backup); } catch (IOException ignored) { }
    }
    void abort() throws IOException { delete(staging); }
    private void delete(Path target) throws IOException {
        Path normalized = target.toAbsolutePath().normalize(), base=root.toAbsolutePath().normalize();
        if (!normalized.getParent().equals(base) || normalized.equals(base)) throw new IOException("Destino de limpeza inválido.");
        if (!Files.exists(normalized)) return;
        try (var paths=Files.walk(normalized)) {
            for (Path path : paths.sorted(Comparator.reverseOrder()).collect(java.util.stream.Collectors.toList())) Files.delete(path);
        }
    }
}
