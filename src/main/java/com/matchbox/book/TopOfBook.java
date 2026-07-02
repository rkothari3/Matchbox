package com.matchbox.book;

public record TopOfBook(long bestBid, long bestAsk, long bidQuantity, long askQuantity) {

    public static TopOfBook empty() {
        return new TopOfBook(Long.MIN_VALUE, Long.MAX_VALUE, 0, 0);
    }

    public boolean hasBid() { return bestBid != Long.MIN_VALUE; }
    public boolean hasAsk() { return bestAsk != Long.MAX_VALUE; }
    public boolean hasBoth() { return hasBid() && hasAsk(); }
    public long spread() { return hasBoth() ? bestAsk - bestBid : Long.MAX_VALUE; }
}
