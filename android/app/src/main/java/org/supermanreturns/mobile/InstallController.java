package org.supermanreturns.mobile;

import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.StatFs;
import java.io.IOException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

final class InstallController {
    record State(boolean busy, boolean installed, int percent, String message) {}
    interface Listener { void changed(State state); }
    private static InstallController instance;
    static synchronized InstallController get(Context context) {
        if (instance == null) instance = new InstallController(context.getApplicationContext());
        return instance;
    }
    private final Context context;
    private final InstallStore store;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final AtomicBoolean cancelled = new AtomicBoolean();
    private volatile State state = new State(true, false, -1, "Verificando instalação…");
    private Listener listener;
    private InstallController(Context context) {
        this.context=context; store=new InstallStore(context.getFilesDir().toPath());
        worker.execute(() -> {
            try { store.recover(); if (java.nio.file.Files.exists(store.active)) { GameFiles.validate(store.active); publish(new State(false,true,100,"Arquivos do jogo verificados.")); }
                else publish(new State(false,false,0,"Selecione sua ISO de Superman Returns para preparar os arquivos."));
            } catch (IOException e) { publish(new State(false,false,0,"Instalação não validada: " + e.getMessage())); }
        });
    }
    void attach(Listener listener) { this.listener=listener; listener.changed(state); }
    void detach(Listener listener) { if (this.listener==listener) this.listener=null; }
    void cancel() { cancelled.set(true); }
    private void publish(State next) {
        state=next; main.post(() -> { if(listener!=null) listener.changed(state); });
    }
    void install(Uri uri) {
        if(state.busy()) return;
        boolean prior=state.installed(); cancelled.set(false);
        publish(new State(true,prior,-1,"Analisando ISO…"));
        worker.execute(() -> {
            try {
                store.prepare();
                ParcelFileDescriptor descriptor=context.getContentResolver().openFileDescriptor(uri,"r");
                if(descriptor==null) throw new IOException("Não foi possível abrir o arquivo.");
                try(ParcelFileDescriptor.AutoCloseInputStream input=new ParcelFileDescriptor.AutoCloseInputStream(descriptor)) {
                        var channel=input.getChannel(); var inspection=XboxIso.inspect(channel);
                        long available=new StatFs(context.getFilesDir().getAbsolutePath()).getAvailableBytes();
                        if(available < inspection.total()+256L*1024*1024) throw new IOException("Espaço insuficiente para instalar sem substituir a cópia atual.");
                        long[] last={0};
                        XboxIso.extract(channel,inspection,store.staging,cancelled::get,(copied,total,file) -> {
                            long now=android.os.SystemClock.uptimeMillis();
                            if(now-last[0]>=200 || copied==total) { last[0]=now;
                                publish(new State(true,prior,(int)(copied*100/total),"Importando " + file)); }
                        });
                }
                if(cancelled.get()) throw new IOException("Importação cancelada.");
                publish(new State(true,prior,100,"Conferindo SHA-256 e arquivos…"));
                store.commit();
                publish(new State(false,true,100,"ISO importada e default.xex verificado. A execução do jogo ainda está em desenvolvimento."));
            } catch (Exception e) {
                try { store.abort(); } catch(IOException cleanup) { android.util.Log.w("SupermanMobile","Import cleanup",cleanup); }
                publish(new State(false,prior,0,"Importação interrompida: " + e.getMessage()));
            }
        });
    }
}
