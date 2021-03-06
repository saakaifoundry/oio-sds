# Copyright (C) 2015 OpenIO, original work as part of
# OpenIO Software Defined Storage
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 3.0 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library.

from oio.common.exceptions import OrphanChunk
from oio.content.content import Content, Chunk
from oio.api.ec import ECChunkDownloadHandler, ECWriteHandler, ECRebuildHandler
from oio.api.object_storage import _sort_chunks, get_meta_ranges


class ECContent(Content):
    def rebuild_chunk(self, chunk_id):
        current_chunk = self.chunks.filter(id=chunk_id).one()

        if current_chunk is None:
            raise OrphanChunk("Chunk not found in content")

        chunks = self.chunks.filter(metapos=current_chunk.metapos)\
            .exclude(id=chunk_id)

        spare_url = self._get_spare_chunk(chunks.all(), [current_chunk])

        handler = ECRebuildHandler(
            chunks.raw(), current_chunk.subpos, self.storage_method)

        new_chunk = {'pos': current_chunk.pos, 'url': spare_url[0]}
        new_chunk = Chunk(new_chunk)
        stream = handler.rebuild()

        meta = {}
        meta['chunk_id'] = new_chunk.id
        meta['chunk_pos'] = current_chunk.pos
        meta['container_id'] = self.container_id
        meta['content_chunkmethod'] = self.chunk_method
        meta['content_id'] = self.content_id
        meta['content_path'] = self.path
        meta['content_policy'] = self.stgpol
        meta['content_version'] = self.version
        meta['metachunk_hash'] = current_chunk.checksum
        meta['metachunk_size'] = current_chunk.size
        self.blob_client.chunk_put(spare_url[0], meta, stream)
        self._update_spare_chunk(current_chunk, spare_url[0])

    def fetch(self):
        chunks = _sort_chunks(self.chunks.raw(), self.storage_method.ec)
        headers = {}
        stream = self._fetch_stream(chunks, self.storage_method, headers)
        return stream

    def _fetch_stream(self, chunks, storage_method, headers):
        meta_ranges = get_meta_ranges([(None, None)], chunks)
        for pos, meta_range in meta_ranges.iteritems():
            meta_start, meta_end = meta_range
            handler = ECChunkDownloadHandler(
                storage_method, chunks[pos], meta_start, meta_end, headers)
            stream = handler = handler.get_stream()
            for part_info in stream:
                for d in part_info['iter']:
                    yield d
            stream.close()

    def create(self, stream):
        sysmeta = {}
        sysmeta['id'] = self.content_id
        sysmeta['version'] = self.version
        sysmeta['policy'] = self.stgpol
        sysmeta['mime_type'] = self.mime_type
        sysmeta['chunk_method'] = self.chunk_method
        sysmeta['chunk_size'] = self.metadata['chunk-size']

        chunks = _sort_chunks(self.chunks.raw(), self.storage_method.ec)
        sysmeta['content_path'] = self.path
        sysmeta['container_id'] = self.container_id

        headers = {}
        handler = ECWriteHandler(
            stream, sysmeta, chunks, self.storage_method, headers=headers)

        final_chunks, bytes_transferred, content_checksum = handler.stream()

        # TODO sanity checks

        self.checksum = content_checksum
        self._create_object()
        return final_chunks, bytes_transferred, content_checksum
