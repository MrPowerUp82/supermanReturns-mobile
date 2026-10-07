package org.supermanreturns.mobile;

import java.io.EOFException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.BooleanSupplier;

/** XDVDFS reader. Partition locations follow the Skate3-Mobile installer reference. */
final class XboxIso {
    private static final int SECTOR = 2048;
    private static final byte[] MAGIC = "MICROSOFT*XBOX*MEDIA".getBytes(StandardCharsets.US_ASCII);
    private static final long[] OFFSETS = {0, 0xFB20L, 0x20600L, 0x2080000L, 0xFD90000L};
    record Entry(String path, long offset, long size) {}
    record Inspection(List<Entry> files, long total) {}
    private record Node(long table, long length, int node, String prefix, int depth) {}
    interface Progress { void update(long copied, long total, String file) throws IOException; }

    static Inspection inspect(FileChannel channel) throws IOException {
        long size = channel.size(), partition = -1;
        for (long offset : OFFSETS) {
            if (offset + 32L * SECTOR + MAGIC.length > size) continue;
            if (java.util.Arrays.equals(read(channel, offset + 32L * SECTOR, MAGIC.length).array(), MAGIC)) { partition = offset; break; }
        }
        if (partition < 0) throw new IOException("ISO Xbox 360 não reconhecida. Selecione um dump XDVDFS local.");
        ByteBuffer root = read(channel, partition + 32L * SECTOR + 20, 8);
        long sector = Integer.toUnsignedLong(root.getInt()), length = Integer.toUnsignedLong(root.getInt());
        checkRange(partition + sector * SECTOR, length, size);
        if (length < 14 || length > 32L * 1024 * 1024) throw new IOException("Diretório raiz inválido.");
        ArrayDeque<Node> pending = new ArrayDeque<>();
        pending.push(new Node(partition + sector * SECTOR, length, 0, "", 0));
        Set<String> visited = new HashSet<>(), names = new HashSet<>();
        List<Entry> files = new ArrayList<>(); long total = 0; int count = 0;
        while (!pending.isEmpty()) {
            Node node = pending.pop();
            if (++count > 100_000 || node.depth > 32 || !visited.add(node.table + ":" + node.node))
                throw new IOException("Árvore de diretórios inválida ou cíclica.");
            if (node.node < 0 || node.node + 14L > node.length) throw new IOException("Entrada fora do diretório.");
            ByteBuffer header = read(channel, node.table + node.node, 14);
            int left = Short.toUnsignedInt(header.getShort()), right = Short.toUnsignedInt(header.getShort());
            long entrySector = Integer.toUnsignedLong(header.getInt()), entrySize = Integer.toUnsignedLong(header.getInt());
            int flags = Byte.toUnsignedInt(header.get()), nameSize = Byte.toUnsignedInt(header.get());
            if (nameSize == 0 || nameSize > 240 || node.node + 14L + nameSize > node.length)
                throw new IOException("Nome de arquivo inválido.");
            String name = new String(read(channel, node.table + node.node + 14, nameSize).array(), StandardCharsets.US_ASCII);
            if (name.equals(".") || name.equals("..") || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 || name.indexOf(':') >= 0 || name.indexOf('\0') >= 0)
                throw new IOException("Caminho inseguro na ISO.");
            if (left != 0) pending.push(new Node(node.table, node.length, left * 4, node.prefix, node.depth));
            if (right != 0) pending.push(new Node(node.table, node.length, right * 4, node.prefix, node.depth));
            long offset = partition + entrySector * SECTOR;
            checkRange(offset, entrySize, size);
            if ((flags & 0x10) != 0) {
                if (entrySize != 0) pending.push(new Node(offset, entrySize, 0, node.prefix + name + "/", node.depth + 1));
            } else {
                String canonical = GameFiles.canonical(node.prefix + name);
                if (canonical != null) {
                    if (!names.add(canonical)) throw new IOException("Arquivo duplicado: " + canonical);
                    if (entrySize == 0) throw new IOException("Arquivo vazio: " + canonical);
                    files.add(new Entry(canonical, offset, entrySize)); total = Math.addExact(total, entrySize);
                }
            }
        }
        if (files.size() != 13) throw new IOException("A ISO precisa conter default.xex e os 12 arquivos DATA/*.AST de Superman Returns.");
        return new Inspection(List.copyOf(files), total);
    }

    static void extract(FileChannel input, Inspection plan, Path destination, BooleanSupplier cancelled, Progress progress) throws IOException {
        Path root = destination.toAbsolutePath().normalize(); Files.createDirectories(root);
        ByteBuffer buffer = ByteBuffer.allocateDirect(1024 * 1024); long copied = 0;
        for (Entry entry : plan.files) {
            Path target = root.resolve(entry.path).normalize();
            if (!target.startsWith(root) || GameFiles.canonical(entry.path) == null) throw new IOException("Caminho de destino inválido.");
            Files.createDirectories(target.getParent());
            try (FileChannel output = FileChannel.open(target, StandardOpenOption.CREATE_NEW, StandardOpenOption.WRITE)) {
                long position = entry.offset, remaining = entry.size;
                while (remaining > 0) {
                    if (cancelled.getAsBoolean()) throw new IOException("Importação cancelada.");
                    buffer.clear(); buffer.limit((int)Math.min(buffer.capacity(), remaining));
                    readFully(input, position, buffer); buffer.flip(); int n = buffer.remaining();
                    while (buffer.hasRemaining()) output.write(buffer);
                    remaining -= n; position += n; copied += n; progress.update(copied, plan.total, entry.path);
                }
                output.force(true);
            }
        }
    }

    private static void checkRange(long offset, long length, long size) throws IOException {
        if (offset < 0 || length < 0 || offset > size || length > size - offset) throw new IOException("Arquivo fora dos limites da ISO.");
    }
    private static ByteBuffer read(FileChannel channel, long offset, int size) throws IOException {
        ByteBuffer b = ByteBuffer.allocate(size).order(ByteOrder.LITTLE_ENDIAN); readFully(channel, offset, b); b.flip(); return b;
    }
    private static void readFully(FileChannel channel, long offset, ByteBuffer buffer) throws IOException {
        while (buffer.hasRemaining()) {
            int n = channel.read(buffer, offset);
            if (n < 0) throw new EOFException("ISO truncada.");
            if (n == 0) throw new IOException("Não foi possível ler a ISO. Copie para o armazenamento local.");
            offset += n;
        }
    }
}
