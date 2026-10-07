package org.supermanreturns.mobile;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.List;

public final class ImporterTests {
    private static int passed;
    private interface Checked { void run() throws Exception; }
    private static void test(String name,Checked code) throws Exception { code.run();passed++;System.out.println("PASS " + name); }
    private static void rejects(Checked code) throws Exception { try {code.run();}catch(IOException expected){return;}throw new AssertionError("Expected IOException"); }
    private static void require(boolean b) {if(!b)throw new AssertionError();}
    private static ByteBuffer iso(long partition) {
        ByteBuffer b=ByteBuffer.allocate((int)partition+120*2048).order(ByteOrder.LITTLE_ENDIAN);
        b.position((int)partition+32*2048);b.put("MICROSOFT*XBOX*MEDIA".getBytes(StandardCharsets.US_ASCII));
        b.putInt(40);b.putInt(2048);
        node(b,(int)partition+40*2048,0,16,0,50,4,0,"DEFAULT.XEX");
        node(b,(int)partition+40*2048,64,0,0,41,2048,0x10,"data");
        for(int i=0;i<12;i++)node(b,(int)partition+41*2048,i*64,i==11?0:(i+1)*16,0,51+i,4,0,GameFiles.ARCHIVES.get(i)+".ast");
        for(int i=50;i<63;i++)b.putInt((int)partition+i*2048,0x12345678);
        return b;
    }
    private static void node(ByteBuffer b,int table,int offset,int right,int left,int sector,int size,int flags,String name) {
        b.position(table+offset);b.putShort((short)left).putShort((short)right).putInt(sector).putInt(size).put((byte)flags).put((byte)name.length()).put(name.getBytes(StandardCharsets.US_ASCII));
    }
    private static void withIso(ByteBuffer data,CheckedChannel code) throws Exception {
        Path path=Files.createTempFile("sr-iso-", ".iso");
        try { Files.write(path,data.array());try(FileChannel channel=FileChannel.open(path,StandardOpenOption.READ)){code.run(channel);} }
        finally {Files.deleteIfExists(path);}
    }
    private interface CheckedChannel {void run(FileChannel channel)throws Exception;}
    private static Path temp;
    public static void main(String[] args) throws Exception {
        temp=Files.createTempDirectory("sr-import-tests-");
        try {
            test("XDVDFS at partition 0 and canonical Android names",() -> withIso(iso(0),c -> {
                var plan=XboxIso.inspect(c);require(plan.files().size()==13 && plan.total()==52);
                Path target=temp.resolve("extract"); final long[] copied={0};
                XboxIso.extract(c,plan,target,()->false,(n,t,f)->copied[0]=n);
                require(copied[0]==52 && Files.size(target.resolve("DATA/voice.AST"))==4);
                require(Files.exists(target.resolve("default.xex")));rejects(()->GameFiles.validate(target));
            }));
            test("XGD game partition offsets",() -> withIso(iso(0xFB20),c -> require(XboxIso.inspect(c).files().size()==13)));
            test("truncated volume rejected",() -> withIso(ByteBuffer.allocate(64),c -> rejects(()->XboxIso.inspect(c))));
            test("directory cycle rejected",() -> {var b=iso(0);b.putShort(40*2048+64,(short)16);withIso(b,c -> rejects(()->XboxIso.inspect(c)));});
            test("path traversal rejected",() -> {var b=iso(0);node(b,40*2048,0,16,0,50,4,0,"../bad.xex");withIso(b,c -> rejects(()->XboxIso.inspect(c)));});
            test("file outside ISO rejected",() -> {var b=iso(0);b.putInt(40*2048+4,5000);withIso(b,c -> rejects(()->XboxIso.inspect(c)));});
            test("node outside declared table rejected",() -> {var b=iso(0);b.putShort(40*2048+2,(short)2000);withIso(b,c -> rejects(()->XboxIso.inspect(c)));});
            test("missing required AST rejected",() -> {var b=iso(0);node(b,41*2048,11*64,0,0,62,4,0,"other.ast");withIso(b,c -> rejects(()->XboxIso.inspect(c)));});
            test("duplicate canonical file rejected",() -> {var b=iso(0);node(b,41*2048,11*64,0,0,62,4,0,"ui.ast");withIso(b,c -> rejects(()->XboxIso.inspect(c)));});
            test("cancellation stops extraction",() -> withIso(iso(0),c -> rejects(()->XboxIso.extract(c,XboxIso.inspect(c),temp.resolve("cancelled"),()->true,(n,t,f)->{}))));
            test("invalid import preserves active installation",() -> {
                InstallStore store=new InstallStore(temp.resolve("store"));store.prepare();Files.createDirectories(store.active);
                Files.writeString(store.active.resolve("sentinel"),"existing");Files.writeString(store.staging.resolve("default.xex"),"wrong");
                rejects(store::commit);store.abort();require(Files.readString(store.active.resolve("sentinel")).equals("existing"));
            });
            test("interrupted promotion restores backup",() -> {
                InstallStore store=new InstallStore(temp.resolve("recovery"));Files.createDirectories(store.backup);Files.writeString(store.backup.resolve("sentinel"),"old");
                store.recover();require(Files.readString(store.active.resolve("sentinel")).equals("old") && !Files.exists(store.backup));
            });
            test("known SHA-256",() -> {Path p=temp.resolve("sha");Files.writeString(p,"abc");require(GameFiles.sha256(p).equals("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));});
            if(args.length>0) {
                Path retail=Path.of(args[0]);
                test("real local Superman Returns game matches pinned profile",()->GameFiles.validate(retail));
                test("valid install promotes staging and retires backup",() -> {
                    InstallStore store=new InstallStore(temp.resolve("valid"));store.prepare();
                    Files.createDirectories(store.active);Files.writeString(store.active.resolve("sentinel"),"previous");
                    Files.createLink(store.staging.resolve("default.xex"),retail.resolve("default.xex"));
                    Files.createDirectories(store.staging.resolve("DATA"));
                    for(String name:GameFiles.ARCHIVES) Files.createLink(store.staging.resolve("DATA/"+name+".AST"),retail.resolve("DATA/"+name+".AST"));
                    store.commit();GameFiles.validate(store.active);
                    require(!Files.exists(store.staging) && !Files.exists(store.backup) && !Files.exists(store.active.resolve("sentinel")));
                });
            }
            System.out.println(passed+" tests passed");
        } finally {
            try(var paths=Files.walk(temp)) {for(Path p:paths.sorted(java.util.Comparator.reverseOrder()).toList())Files.delete(p);}
        }
    }
}
