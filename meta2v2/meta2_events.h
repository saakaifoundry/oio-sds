#ifndef META2V2_EVENTS__H
# define META2V2_EVENTS__H 1
# ifndef G_LOG_DOMAIN
#  define G_LOG_DOMAIN "m2"
# endif



# ifndef  META2_EVTFIELD_NAMESPACE
#  define META2_EVTFIELD_NAMESPACE "NS"
# endif
# ifndef  META2_EVTFIELD_CNAME
#  define META2_EVTFIELD_CNAME "CNAME"
# endif
# ifndef  META2_EVTFIELD_CPATH
#  define META2_EVTFIELD_CPATH "CPATH"
# endif
# ifndef  META2_EVTFIELD_CID
#  define META2_EVTFIELD_CID GRIDCLUSTER_EVTFIELD_CID
# endif
# ifndef  META2_EVTFIELD_UEID
#  define META2_EVTFIELD_UEID GRIDCLUSTER_EVTFIELD_UEID
# endif
# ifndef  META2_EVTFIELD_AGGRNAME
#  define META2_EVTFIELD_AGGRNAME GRIDCLUSTER_EVTFIELD_AGGRNAME
# endif
# ifndef  META2_EVTFIELD_RAWCONTENT
#  define META2_EVTFIELD_RAWCONTENT "RAW"
# endif
# ifndef  META2_EVTFIELD_RAWCONTENT_V2
#  define META2_EVTFIELD_RAWCONTENT_V2 "RAW.V2"
# endif
# ifndef META2_EVTFIELD_CEVT
#  define META2_EVTFIELD_CEVT "CEVT"
# endif
# ifndef META2_EVTFIELD_CEVTID
#  define META2_EVTFIELD_CEVTID "CEVTID"
# endif
# ifndef META2_EVTFIELD_URL
#  define META2_EVTFIELD_URL "URL"
# endif

# ifndef  META2_EVTTYPE_CREATE
#  define META2_EVTTYPE_CREATE "meta2.CONTAINER.create"
# endif
# ifndef  META2_EVTTYPE_DESTROY
#  define META2_EVTTYPE_DESTROY "meta2.CONTAINER.destroy"
# endif
# ifndef  META2_EVTTYPE_PUT
#  define META2_EVTTYPE_PUT "meta2.CONTENT.put"
# endif
# ifndef  META2_EVTTYPE_DELETE
#  define META2_EVTTYPE_DELETE "meta2.CONTENT.delete"
# endif

# ifndef  META2_EVTTYPE_CONTENT_PROPSET
#  define META2_EVTTYPE_CONTENT_PROPSET "meta2.CONTENT.prop.set"
# endif
# ifndef  META2_EVTTYPE_CONTENT_PROPDEL
#  define META2_EVTTYPE_CONTENT_PROPDEL "meta2.CONTENT.prop.del"
# endif
# ifndef  META2_EVTTYPE_CONTAINER_PROPSET
#  define META2_EVTTYPE_CONTAINER_PROPSET "meta2.CONTAINER.prop.set"
# endif
# ifndef  META2_EVTTYPE_CONTAINER_PROPDEL
#  define META2_EVTTYPE_CONTAINER_PROPDEL "meta2.CONTAINER.prop.del"
# endif
# ifndef  META2_EVTTYPE_CONTAINER_EVTADD
#  define META2_EVTTYPE_CONTAINER_EVTADD "meta2.CONTAINER.evt.add"
# endif
# ifndef  META2_EVTTYPE_CONTAINER_EVTDIFF
#  define META2_EVTTYPE_CONTAINER_EVTDIFF "meta2.CONTAINER.evt.diff"
# endif
# ifndef  META2_EVTTYPE_CONTAINER_EVTRM
#  define META2_EVTTYPE_CONTAINER_EVTRM "meta2.CONTAINER.evt.rm"
# endif

#endif /* META2V2_EVENTS__H */